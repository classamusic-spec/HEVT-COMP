// heat_gpucheck — verifies the OpenGL meter against the CPU meter.
//
//   heat_gpucheck <outDir> [scale=1.0] [grDb=-3.66] [peakDb=-6]
//
// Opens the real editor in a window with the GPU meter enabled, waits until
// the shader is live, then reads back
//   (a) the meter glass exactly as the GPU renderer drew it, and
//   (b) a complete presented frame (GPU meter + JUCE's GL-composited UI),
// and compares them with the CPU renderings of the same state:
//   (a') GainReductionDisplay::paintBackground + paintDynamic + paintNeedle
//   (b') the editor rendered in software with the CPU meter.
// Writes PNGs and prints mean / 99th-percentile / max absolute differences.
// Exit code 0 = pass, 1 = fail, 2 = no usable OpenGL (UNVERIFIED).

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "UI/GainReductionDisplay.h"
#include "UI/GpuMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    struct Diff
    {
        double mae = 0.0, p99 = 0.0, max = 0.0;
        int pixels = 0;
    };

    // Per-channel absolute RGB difference over the pixels where mask(x, y) is true.
    template <typename Mask>
    Diff compare (const juce::Image& a, const juce::Image& b, Mask mask)
    {
        Diff d;
        std::vector<int> hist (256, 0);
        long long sum = 0, count = 0;
        const int w = std::min (a.getWidth(), b.getWidth()), h = std::min (a.getHeight(), b.getHeight());
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                if (! mask (x, y))
                    continue;
                const auto ca = a.getPixelAt (x, y), cb = b.getPixelAt (x, y);
                for (int e : { std::abs (ca.getRed() - cb.getRed()), std::abs (ca.getGreen() - cb.getGreen()), std::abs (ca.getBlue() - cb.getBlue()) })
                {
                    sum += e;
                    ++count;
                    ++hist[static_cast<size_t> (e)];
                    d.max = std::max (d.max, static_cast<double> (e));
                }
            }
        d.pixels = static_cast<int> (count / 3);
        d.mae = count > 0 ? static_cast<double> (sum) / static_cast<double> (count) : 0.0;
        long long acc = 0;
        for (int e = 0; e < 256; ++e)
        {
            acc += hist[static_cast<size_t> (e)];
            if (acc >= static_cast<long long> (0.99 * static_cast<double> (count)))
            {
                d.p99 = e;
                break;
            }
        }
        return d;
    }

    juce::Image sideBySide (const juce::Image& gpu, const juce::Image& cpu, int amplify)
    {
        const int w = std::min (gpu.getWidth(), cpu.getWidth()), h = std::min (gpu.getHeight(), cpu.getHeight());
        juce::Image out (juce::Image::RGB, 3 * w + 16, h, true);
        juce::Graphics g (out);
        g.fillAll (juce::Colours::black);
        g.drawImageAt (gpu, 0, 0);
        g.drawImageAt (cpu, w + 8, 0);
        juce::Image diff (juce::Image::RGB, w, h, true);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto a = gpu.getPixelAt (x, y), b = cpu.getPixelAt (x, y);
                auto amp = [amplify] (int v) { return static_cast<juce::uint8> (std::min (255, v * amplify)); };
                diff.setPixelAt (x, y, juce::Colour (amp (std::abs (a.getRed() - b.getRed())), amp (std::abs (a.getGreen() - b.getGreen())),
                                                     amp (std::abs (a.getBlue() - b.getBlue()))));
            }
        g.drawImageAt (diff, 2 * w + 16, 0);
        return out;
    }

    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat png;
        return stream.openedOk() && png.writeImageToStream (image, stream);
    }
}

class GpuCheckApp final : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "heat_gpucheck"; }
    const juce::String getApplicationVersion() override { return HEAT_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        args.removeEmptyStrings();
        outDir = juce::File::getCurrentWorkingDirectory().getChildFile (args.size() > 0 ? args[0] : "gpucheck");
        outDir.createDirectory();
        scale = args.size() > 1 ? args[1].getFloatValue() : 1.0f;
        grDb = args.size() > 2 ? args[2].getFloatValue() : -3.66f;
        peakDb = args.size() > 3 ? args[3].getFloatValue() : -6.0f;

        presetDir = std::make_unique<juce::TemporaryFile> ("heat_gpucheck");
        presetDir->getFile().createDirectory();
        processor = std::make_unique<HeatAudioProcessor> (presetDir->getFile());
        processor->setGpuMeter (true);
        editor.reset (dynamic_cast<HeatAudioProcessorEditor*> (processor->createEditorAndMakeActive()));
        editor->setSize (juce::roundToInt (1536.0f * scale), juce::roundToInt (1024.0f * scale));

        window = std::make_unique<juce::DocumentWindow> ("HEAT GPU check", juce::Colours::black, 0);
        window->setUsingNativeTitleBar (true);
        window->setContentNonOwned (editor.get(), true);
        window->setTopLeftPosition (0, 0);
        window->setVisible (true);

        started = juce::Time::getMillisecondCounterHiRes();
        startTimer (40);
    }

    void shutdown() override
    {
        stopTimer();
        if (editor != nullptr)
        {
            editor->setGpuMeterEnabled (false);
            window->clearContentComponent();
            processor->editorBeingDeleted (editor.get());
            editor.reset();
        }
        window.reset();
        processor.reset();
        if (presetDir != nullptr)
            presetDir->getFile().deleteRecursively();
    }

    void systemRequestedQuit() override { quit(); }

private:
    enum class Step { waitGpu, settle, capture, done };

    void setState()
    {
        auto& meter = editor->getMainPanel().getMeter();
        meter.setImmediate (grDb, grDb + 0.8f, peakDb);
    }

    void timerCallback() override
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - started;
        if (elapsed > 20000.0)
        {
            std::printf ("GPU meter: UNVERIFIED - no usable OpenGL context within 20 s (active %d)\n", editor->isGpuMeterActive() ? 1 : 0);
            finish (2);
            return;
        }

        switch (step)
        {
            case Step::waitGpu:
                if (editor->isGpuMeterActive())
                {
                    setState();
                    step = Step::settle;
                    settleTicks = 0;
                }
                else if (elapsed > 5000.0 && editor->getGpuRenderer() == nullptr)
                {
                    std::printf ("GPU meter: UNVERIFIED - OpenGL unavailable, the editor fell back to the CPU meter\n");
                    finish (2);
                }
                break;

            case Step::settle:
                if (++settleTicks >= 10)
                {
                    auto* r = editor->getGpuRenderer();
                    r->requestGlassCapture ([this] (juce::Image img)
                    {
                        const juce::SpinLock::ScopedLockType l (lock);
                        gpuGlass = img.createCopy();
                    });
                    r->requestFrameCapture ([this] (juce::Image img)
                    {
                        const juce::SpinLock::ScopedLockType l (lock);
                        gpuFrame = img.createCopy();
                    });
                    step = Step::capture;
                }
                setState();
                break;

            case Step::capture:
            {
                setState();
                editor->getMainPanel().repaint();
                juce::Image glass, frame;
                {
                    const juce::SpinLock::ScopedLockType l (lock);
                    glass = gpuGlass;
                    frame = gpuFrame;
                }
                if (glass.isValid() && frame.isValid())
                {
                    step = Step::done;
                    finish (evaluate (glass, frame) ? 0 : 1);
                }
                break;
            }

            case Step::done:
                break;
        }
    }

    bool evaluate (const juce::Image& gpuGlassImage, const juce::Image& gpuFrameImage)
    {
        using heat::ui::GainReductionDisplay;
        const float renderScale = static_cast<float> (gpuFrameImage.getWidth()) / static_cast<float> (editor->getWidth());
        const float px = scale * renderScale;

        // (a') CPU reference of the glass region, same pixel grid as the capture.
        const auto glassRef = GainReductionDisplay::glassBounds();
        const int x0 = static_cast<int> (std::floor (glassRef.getX() * px));
        const int y0 = static_cast<int> (std::floor (glassRef.getY() * px));
        juce::Image cpuGlass (juce::Image::ARGB, gpuGlassImage.getWidth(), gpuGlassImage.getHeight(), true);
        {
            juce::Graphics g (cpuGlass);
            g.addTransform (juce::AffineTransform::scale (px).translated (static_cast<float> (-x0), static_cast<float> (-y0)));
            GainReductionDisplay::paintBackground (g);
            GainReductionDisplay::paintDynamic (g, grDb, grDb + 0.8f);
            GainReductionDisplay::paintNeedle (g, peakDb);
        }
        // Inside the glass, 2 px away from its antialiased outline.
        juce::Path inner;
        inner.addRoundedRectangle (glassRef.reduced (2.0f / px), GainReductionDisplay::glassRadius() - 2.0f / px);
        const auto toRef = juce::AffineTransform::translation (static_cast<float> (x0) + 0.5f, static_cast<float> (y0) + 0.5f).scaled (1.0f / px);
        const auto glassDiff = compare (gpuGlassImage, cpuGlass, [&] (int x, int y)
        {
            auto p = juce::Point<float> (static_cast<float> (x), static_cast<float> (y)).transformedBy (toRef);
            return inner.contains (p);
        });

        // (b') the full editor in software with the CPU meter.
        editor->setGpuMeterEnabled (false);
        editor->getMainPanel().getMeter().setImmediate (grDb, grDb + 0.8f, peakDb);
        const auto cpuFrame = editor->createComponentSnapshot (editor->getLocalBounds(), true, renderScale);
        const auto frameDiff = compare (gpuFrameImage, cpuFrame, [] (int, int) { return true; });
        const auto meterBox = GainReductionDisplay::glassBounds().expanded (12.0f);
        const auto meterFrameDiff = compare (gpuFrameImage, cpuFrame, [&] (int x, int y)
        {
            return meterBox.contains ((static_cast<float> (x) + 0.5f) / px, (static_cast<float> (y) + 0.5f) / px);
        });

        writePng (gpuGlassImage, outDir.getChildFile ("gpu_glass.png"));
        writePng (cpuGlass, outDir.getChildFile ("cpu_glass.png"));
        writePng (gpuFrameImage, outDir.getChildFile ("gpu_frame.png"));
        writePng (cpuFrame, outDir.getChildFile ("cpu_frame.png"));
        writePng (sideBySide (gpuGlassImage, cpuGlass, 8), outDir.getChildFile ("GPU_vs_CPU_METER.png"));

        std::printf ("GPU meter vs CPU meter @ scale %.2f (render scale %.2f), GR %.2f dB, peak %.1f dB\n", scale, renderScale, grDb, peakDb);
        std::printf ("  glass interior  (%6d px): mean %.2f, p99 %.0f, max %.0f  (/255)\n", glassDiff.pixels, glassDiff.mae, glassDiff.p99, glassDiff.max);
        std::printf ("  meter in frame  (%6d px): mean %.2f, p99 %.0f, max %.0f\n", meterFrameDiff.pixels, meterFrameDiff.mae, meterFrameDiff.p99, meterFrameDiff.max);
        std::printf ("  whole editor    (%6d px): mean %.2f, p99 %.0f, max %.0f\n", frameDiff.pixels, frameDiff.mae, frameDiff.p99, frameDiff.max);

        const bool pass = glassDiff.mae < 1.5 && glassDiff.p99 <= 8.0 && meterFrameDiff.mae < 2.5 && frameDiff.mae < 2.5;
        std::printf ("  result: %s\n", pass ? "PASS" : "FAIL");
        return pass;
    }

    void finish (int code)
    {
        stopTimer();
        setApplicationReturnValue (code);
        quit();
    }

    juce::File outDir;
    float scale = 1.0f, grDb = -3.66f, peakDb = -6.0f;
    std::unique_ptr<juce::TemporaryFile> presetDir;
    std::unique_ptr<HeatAudioProcessor> processor;
    std::unique_ptr<HeatAudioProcessorEditor> editor;
    std::unique_ptr<juce::DocumentWindow> window;
    Step step = Step::waitGpu;
    int settleTicks = 0;
    double started = 0.0;
    juce::SpinLock lock;
    juce::Image gpuGlass, gpuFrame;
};

START_JUCE_APPLICATION (GpuCheckApp)
