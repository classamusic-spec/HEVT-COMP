// heat_snapshot — renders the HEAT editor headlessly to PNG for visual
// comparison with design/reference/HEAT_LOCKED_REFERENCE.png.
//
//   heat_snapshot <out.png> [scale=1.0] [grDb=-3.66] [state=reference|advanced|clean|bench]

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Core/Constants.h"

#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "heat_snapshot.png"));
    const float scale = argc > 2 ? static_cast<float> (std::atof (argv[2])) : 1.0f;
    const float grDb = argc > 3 ? static_cast<float> (std::atof (argv[3])) : -3.66f;
    const juce::String state = argc > 4 ? juce::String (argv[4]) : juce::String ("reference");

    juce::TemporaryFile presetDir ("heat_snapshot");
    presetDir.getFile().createDirectory();

    int result = 0;
    {
        HeatAudioProcessor processor (presetDir.getFile());
        if (state == "clean")
        {
            auto& pm = processor.getPresetManager();
            pm.loadPreset (pm.findByName ("Master 1 dB"));
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        auto* heatEditor = dynamic_cast<HeatAudioProcessorEditor*> (editor.get());
        editor->setSize (juce::roundToInt (1536.0f * scale), juce::roundToInt (1024.0f * scale));
        heatEditor->getMainPanel().setPreviewGainReduction (grDb);
        if (state == "advanced")
            heatEditor->getMainPanel().toggleAdvanced();

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

        if (state == "bench")
        {
            // Frame cost: whole editor, and the per-frame dirty region (meter +
            // COMPRESS knob) that the vblank animation actually repaints.
            auto time = [] (auto&& fn, int n)
            {
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < n; ++i)
                    fn (i);
                return (juce::Time::getMillisecondCounterHiRes() - t0) / n;
            };
            auto& panel = heatEditor->getMainPanel();
            const double full = time ([&] (int) { editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f); }, 30);
            const auto meterArea = juce::Rectangle<int> (627, 137, 285, 419);
            const auto knobArea = juce::Rectangle<int> (600, 545, 340, 340);
            const double animated = time ([&] (int i)
            {
                panel.setPreviewGainReduction (-1.0f - static_cast<float> (i % 20));
                panel.createComponentSnapshot (meterArea, true, scale);
                panel.createComponentSnapshot (knobArea, true, scale);
            }, 200);
            std::printf ("render bench @ scale %.2f: full editor %.2f ms/frame, animated regions %.2f ms/frame\n",
                         scale, full, animated);
        }
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;
        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
            result = 1;
        else
            std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());

        processor.editorBeingDeleted (editor.get());
    }
    presetDir.getFile().deleteRecursively();
    juce::DeletedAtShutdown::deleteAll();
    return result;
}
