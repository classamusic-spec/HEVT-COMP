#include "UI/AdvancedPanel.h"
#include "UI/HeatTheme.h"
#include "Core/Constants.h"
#include "PluginProcessor.h"

namespace heat::ui
{
    namespace
    {
        constexpr float rowHeight = 36.0f;
        constexpr float headerHeight = 50.0f;
        constexpr float tabsHeight = 40.0f;
        constexpr float footerHeight = 44.0f;
        constexpr float labelWidth = 150.0f;
        constexpr float padding = 22.0f;
        constexpr float meterReleaseSeconds = 0.25f;

        // The page the panel was last left on (per process).
        AdvancedPanel::Page lastPage = AdvancedPanel::Page::general;

        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }

        juce::RangedAudioParameter* rangedParam (HeatAudioProcessor& p, const char* id)
        {
            return dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        }
    }

    juce::Rectangle<float> AdvancedPanel::panelBounds()
    {
        const float h = headerHeight + tabsHeight + static_cast<float> (maxRowsPerPage) * rowHeight + footerHeight;
        return { 1022.0f, 124.0f, 490.0f, h };
    }

    AdvancedPanel::AdvancedPanel (HeatAudioProcessor& p)
        : processor (p),
          tabs ({ "GENERAL", "SIDECHAIN", "MULTIBAND", "OUTPUT" }, SegmentedSelector::Style::glass, "Settings page")
    {
        setTitle ("Advanced settings");
        setWantsKeyboardFocus (true);

        tabs.setTooltips ({ "Stereo mode, link, look-ahead, makeup, release, quality, IRON model",
                            "Detector source, listen and EQ",
                            "Multiband compression: crossovers and band amounts",
                            "Output limiter, meter and interface options" });
        tabs.onSelect = [this] (int i) { setPage (static_cast<Page> (i)); };
        addAndMakeVisible (tabs);

        // --- GENERAL ------------------------------------------------------------------
        addChoice (Page::general, "STEREO MODE", heat::stereoModeChoices,
                   { "Left / right channels", "Mid / side: mid and side compressed separately (use with DUAL MONO or PARTIAL link)",
                     "Only the mid (centre) is processed; the side passes untouched",
                     "Only the side (width) is processed; the mid passes untouched" }, ids::stereoMode);
        addChoice (Page::general, "STEREO LINK", heat::linkChoices,
                   { "Identical gain on both channels (stable image)", "Half-linked detection", "Independent channels" }, ids::scLink);
        addChoice (Page::general, "LOOKAHEAD MS", { "OFF", "0.5", "1", "2", "5", "10" },
                   { "No look-ahead (lowest latency)", "0.5 ms look-ahead", "1 ms look-ahead", "2 ms look-ahead",
                     "5 ms look-ahead", "10 ms look-ahead" }, ids::lookahead);
        addChoice (Page::general, "AUTO MAKEUP", { "OFF", "ON" }, { "No automatic makeup gain", "Conservative makeup from the COMPRESS curve" }, ids::autoMakeup);
        addChoice (Page::general, "AUTO RELEASE", { "OFF", "ON" }, { "Release follows the RELEASE control", "Program-dependent two-stage release in every mode" }, ids::autoRelease);
        addChoice (Page::general, "QUALITY", { "NORMAL", "HIGH", "ULTRA" },
                   { "2x oversampling for TUBE / IRON / colour", "4x oversampling (default)", "8x oversampling" }, ids::quality);
        addChoice (Page::general, "IRON MODEL", heat::ironModelChoices,
                   { "HEAT 2.0 transformer colour (sessions from 2.0 keep it)",
                     "Jiles-Atherton hysteresis core: low-level grain, real B-H loop, saturation knee" }, ids::ironModel);

        // --- SIDECHAIN ----------------------------------------------------------------
        addChoice (Page::sidechain, "SOURCE", { "INTERNAL", "EXTERNAL" },
                   { "Detector listens to the main input", "Detector listens to the external sidechain bus" }, ids::scSource);
        addChoice (Page::sidechain, "SC LISTEN", { "OFF", "ON" }, { "Normal output", "Audition the filtered detector signal" }, ids::scListen);
        addSlider (Page::sidechain, "HIGH-PASS", ids::hpf, "Detector high-pass (same as the front-panel HPF)");
        addSlider (Page::sidechain, "LOW-PASS", ids::scLpf, "Detector low-pass (20 kHz = off)");
        addSlider (Page::sidechain, "EQ FREQ", ids::scEqFreq, "Detector bell frequency");
        addSlider (Page::sidechain, "EQ GAIN", ids::scEqGain, "Detector bell gain: + reacts more to that band (de-essing), - less");
        addSlider (Page::sidechain, "EQ Q", ids::scEqQ, "Detector bell width");

        // --- MULTIBAND ----------------------------------------------------------------
        addChoice (Page::multiband, "MULTIBAND", heat::multibandChoices,
                   { "Single-band compression", "Two bands (LOW / HIGH, split at the low crossover)",
                     "Three bands (LOW / MID / HIGH)" }, ids::multiband);
        addSlider (Page::multiband, "LOW X-OVER", ids::xoverLow, "LOW / MID crossover");
        xoverHigh = addSlider (Page::multiband, "HIGH X-OVER", ids::xoverHigh, "MID / HIGH crossover (3 bands)");
        bandSliders[0] = addSlider (Page::multiband, "LOW", ids::bandLow, "How hard the LOW band works (100 % = COMPRESS)");
        bandSliders[1] = addSlider (Page::multiband, "MID", ids::bandMid, "How hard the MID band works (3 bands)");
        bandSliders[2] = addSlider (Page::multiband, "HIGH", ids::bandHigh, "How hard the HIGH band works");

        // --- OUTPUT -------------------------------------------------------------------
        addChoice (Page::output, "LIMITER", { "OFF", "ON" },
                   { "No output limiter", "True-peak limiter after OUTPUT (adds 1.5 ms look-ahead)" }, ids::limiter);
        ceiling = addSlider (Page::output, "CEILING", ids::ceiling, "Limiter true-peak ceiling");
        meterHold = addChoice (Page::output, "METER HOLD", { "OFF", "ON" }, { "Needle follows gain reduction", "Needle holds peaks for 1.5 s" }, nullptr);
        gpuMeter = addChoice (Page::output, "GPU METER", { "OFF", "ON" },
                              { "Meter drawn by the CPU", "Meter drawn by the graphics card (OpenGL)" }, nullptr);
        uiSize = addChoice (Page::output, "UI SIZE", { "75%", "100%", "125%" }, { "1152 x 768", "1536 x 1024", "1920 x 1280" }, nullptr);

        meterHold->onSelect = [this] (int i)
        {
            processor.setPeakHold (i == 1);
            if (onPeakHold)
                onPeakHold (i == 1);
        };
        gpuMeter->onSelect = [this] (int i)
        {
            processor.setGpuMeter (i == 1);
            if (onGpuMeter)
                onGpuMeter (i == 1);
        };
        uiSize->onSelect = [this] (int i)
        {
            if (onUiScale)
                onUiScale (i == 0 ? 0.75f : i == 1 ? 1.0f : 1.25f);
        };

        setBounds (panelBounds().toNearestInt());
        setPage (lastPage);
        syncFromProcessor();
    }

    AdvancedPanel::~AdvancedPanel() = default;

    SegmentedSelector* AdvancedPanel::addChoice (Page pg, const juce::String& label, juce::StringArray options,
                                                 juce::StringArray tips, const char* paramId)
    {
        Row row;
        row.page = pg;
        row.label = label;
        row.selector = std::make_unique<SegmentedSelector> (std::move (options), SegmentedSelector::Style::glass, label);
        row.selector->setTooltips (std::move (tips));
        if (paramId != nullptr)
            if (auto* param = rangedParam (processor, paramId))
                row.selector->bindToParameter (*param, &processor.getUndoManager());
        addChildComponent (*row.selector);
        auto* raw = row.selector.get();
        rows.push_back (std::move (row));
        return raw;
    }

    AdvancedSlider* AdvancedPanel::addSlider (Page pg, const juce::String& label, const char* paramId, const juce::String& tip)
    {
        Row row;
        row.page = pg;
        row.label = label;
        auto* param = rangedParam (processor, paramId);
        jassert (param != nullptr);
        row.slider = std::make_unique<AdvancedSlider> (*param, &processor.getUndoManager(), label);
        row.slider->setTooltip (tip);
        addChildComponent (*row.slider);
        auto* raw = row.slider.get();
        rows.push_back (std::move (row));
        return raw;
    }

    void AdvancedPanel::setPage (Page pg)
    {
        page = pg;
        lastPage = pg;
        tabs.setSelectedIndex (static_cast<int> (pg), false);
        for (auto& r : rows)
            r.control()->setVisible (r.page == pg);
        updateDimming();
        resized();
        repaint();
    }

    void AdvancedPanel::syncFromProcessor()
    {
        if (meterHold != nullptr)
            meterHold->setSelectedIndex (processor.getPeakHold() ? 1 : 0, false);
        if (gpuMeter != nullptr)
            gpuMeter->setSelectedIndex (processor.getGpuMeter() ? 1 : 0, false);
        if (uiSize != nullptr)
        {
            const float s = processor.getUiScale();
            uiSize->setSelectedIndex (s < 0.87f ? 0 : s < 1.12f ? 1 : 2, false);
        }
        updateDimming();
        repaint();
    }

    void AdvancedPanel::updateDimming()
    {
        auto choice = [this] (const char* id)
        {
            if (auto* prm = rangedParam (processor, id))
                return juce::roundToInt (prm->convertFrom0to1 (prm->getValue()));
            return 0;
        };
        const int mb = choice (ids::multiband);
        for (int b = 0; b < 3; ++b)
            if (bandSliders[b] != nullptr)
                bandSliders[b]->setDimmed (mb == 0 || (b == 1 && mb == 1));
        if (xoverHigh != nullptr)
            xoverHigh->setDimmed (mb != 2);
        if (ceiling != nullptr)
            ceiling->setDimmed (choice (ids::limiter) == 0);
    }

    void AdvancedPanel::updateLive (double dt)
    {
        auto& t = processor.getTelemetry();
        const float k = 1.0f - std::exp (-static_cast<float> (dt) / meterReleaseSeconds);
        auto follow = [k] (float& shown, float measured)
        {
            measured = std::min (0.0f, measured);
            shown = measured < shown ? measured : shown + k * (measured - shown);
        };

        for (int b = 0; b < 3; ++b)
        {
            follow (bandShown[b], t.consumeBandGrDb (b));
            if (bandSliders[b] != nullptr)
                bandSliders[b]->setMeterDb (t.isMultibandActive() ? bandShown[b] : 1.0f, 12.0f);
        }
        follow (limiterShown, t.consumeLimiterGrDb());
        if (ceiling != nullptr)
            ceiling->setMeterDb (limiterShown, 6.0f);

        updateDimming();

        const int latency = processor.getLatencySamples();
        if (latency != shownLatency)
        {
            shownLatency = latency;
            repaint (getLocalBounds().removeFromBottom (static_cast<int> (footerHeight)));
        }
    }

    void AdvancedPanel::resized()
    {
        const float inset = SegmentedSelector::insetFor (SegmentedSelector::Style::glass);
        const float width = static_cast<float> (getWidth());
        tabs.setBounds (juce::Rectangle<float> (padding - inset, headerHeight + 2.0f - inset,
                                                width - 2.0f * padding + 2.0f * inset, tabsHeight - 12.0f + 2.0f * inset).toNearestInt());
        tabs.setPillGeometry (0.0f, 6.0f);

        const float controlX = padding + labelWidth;
        const float controlW = width - controlX - padding + 3.0f;
        int index = 0;
        for (auto& r : rows)
        {
            if (r.page != page)
                continue;
            const float y = headerHeight + tabsHeight + static_cast<float> (index++) * rowHeight;
            if (r.selector != nullptr)
            {
                r.selector->setBounds (juce::Rectangle<float> (controlX - inset, y + 4.0f - inset,
                                                               controlW, rowHeight - 8.0f + 2.0f * inset).toNearestInt());
                r.selector->setPillGeometry (0.0f, 6.0f);
            }
            else
            {
                r.slider->setBounds (juce::Rectangle<float> (controlX - 6.0f, y + 2.0f, controlW + 2.0f, rowHeight - 4.0f).toNearestInt());
            }
        }
    }

    bool AdvancedPanel::keyPressed (const juce::KeyPress& key)
    {
        if (key.isKeyCode (juce::KeyPress::escapeKey))
        {
            if (onClose)
                onClose();
            return true;
        }
        return false;
    }

    void AdvancedPanel::paint (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);

        {
            juce::Path p;
            p.addRoundedRectangle (r, 18.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 24, { 0, 8 }).drawForPath (g, p);
            juce::ColourGradient bg (rgb (30, 33, 36, 0.98f), 0.0f, r.getY(), rgb (18, 20, 22, 0.98f), 0.0f, r.getBottom(), false);
            g.setGradientFill (bg);
            g.fillPath (p);
            g.setColour (rgb (200, 203, 206, 0.55f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }

        const TextSpec title { Weight::medium, 10.0f, 3.2f, rgb (226, 229, 232) };
        drawText (g, "ADVANCED", title, padding, 20.0f, juce::Justification::left);
        const float titleWidth = textPath ("ADVANCED", title).getBounds().getWidth();
        drawText (g, "SETTINGS", { Weight::regular, 10.0f, 3.2f, rgb (140, 145, 150) },
                  padding + titleWidth + 12.0f, 20.0f, juce::Justification::left);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (padding, headerHeight - 8.0f, r.getWidth() - 2.0f * padding, 1.0f));
        g.fillRect (juce::Rectangle<float> (padding, headerHeight + tabsHeight - 4.0f, r.getWidth() - 2.0f * padding, 1.0f));

        int index = 0;
        for (auto& row : rows)
        {
            if (row.page != page)
                continue;
            const float y = headerHeight + tabsHeight + static_cast<float> (index++) * rowHeight;
            drawText (g, row.label, { Weight::medium, 8.0f, 2.0f, rgb (170, 175, 180) },
                      padding, y + 0.5f * rowHeight - 4.0f, juce::Justification::left);
        }

        const float fy = static_cast<float> (getHeight()) - footerHeight + 10.0f;
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (padding, fy - 6.0f, r.getWidth() - 2.0f * padding, 1.0f));
        const int latency = processor.getLatencySamples();
        const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
        const auto info = "LATENCY " + juce::String (latency) + " SAMPLES (" + juce::String (1000.0 * latency / rate, 1) + " MS)   "
                          + juce::String::fromUTF8 ("\xc2\xb7") + "   HEAT " + juce::String (HEAT_VERSION_STRING);
        drawText (g, info, { Weight::regular, 7.0f, 1.6f, rgb (120, 125, 130) },
                  padding, fy + 8.0f, juce::Justification::left);
    }
}
