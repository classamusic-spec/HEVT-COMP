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
        constexpr float footerHeight = 40.0f;
        constexpr float labelWidth = 150.0f;
        constexpr float padding = 22.0f;

        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }
    }

    juce::Rectangle<float> AdvancedPanel::panelBounds()
    {
        const float h = headerHeight + 8.0f * rowHeight + footerHeight;
        return { 1022.0f, 124.0f, 490.0f, h };
    }

    AdvancedPanel::AdvancedPanel (HeatAudioProcessor& p) : processor (p)
    {
        setTitle ("Advanced settings");
        setWantsKeyboardFocus (true);

        addRow ("SIDECHAIN", { "INTERNAL", "EXTERNAL" },
                { "Detector listens to the main input", "Detector listens to the external sidechain bus" }, ids::scSource);
        addRow ("SC LISTEN", { "OFF", "ON" }, { "Normal output", "Audition the filtered detector signal" }, ids::scListen);
        addRow ("STEREO LINK", { "LINKED", "PARTIAL", "DUAL MONO" },
                { "Identical gain on both channels (stable image)", "Half-linked detection", "Independent channels" }, ids::scLink);
        addRow ("AUTO MAKEUP", { "OFF", "ON" }, { "No automatic makeup gain", "Conservative makeup from the COMPRESS curve" }, ids::autoMakeup);
        addRow ("AUTO RELEASE", { "OFF", "ON" }, { "Release follows the RELEASE control", "Program-dependent two-stage release in every mode" }, ids::autoRelease);
        addRow ("QUALITY", { "NORMAL", "HIGH", "ULTRA" },
                { "2x oversampling for TUBE / IRON / colour", "4x oversampling (default)", "8x oversampling" }, ids::quality);
        addRow ("METER HOLD", { "OFF", "ON" }, { "Needle follows gain reduction", "Needle holds peaks for 1.5 s" }, nullptr);
        addRow ("UI SIZE", { "75%", "100%", "125%" }, { "1152 x 768", "1536 x 1024", "1920 x 1280" }, nullptr);

        meterHold = rows[6].selector.get();
        uiSize = rows[7].selector.get();
        meterHold->onSelect = [this] (int i)
        {
            processor.setPeakHold (i == 1);
            if (onPeakHold)
                onPeakHold (i == 1);
        };
        uiSize->onSelect = [this] (int i)
        {
            if (onUiScale)
                onUiScale (i == 0 ? 0.75f : i == 1 ? 1.0f : 1.25f);
        };

        setBounds (panelBounds().toNearestInt());
        syncFromProcessor();
    }

    void AdvancedPanel::addRow (const juce::String& label, juce::StringArray options, juce::StringArray tips,
                                const char* paramId)
    {
        Row row;
        row.label = label;
        row.selector = std::make_unique<SegmentedSelector> (std::move (options), SegmentedSelector::Style::glass, label);
        row.selector->setTooltips (std::move (tips));
        if (paramId != nullptr)
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (paramId)))
                row.selector->bindToParameter (*param, &processor.getUndoManager());
        addAndMakeVisible (*row.selector);
        rows.push_back (std::move (row));
    }

    void AdvancedPanel::syncFromProcessor()
    {
        if (meterHold != nullptr)
            meterHold->setSelectedIndex (processor.getPeakHold() ? 1 : 0, false);
        if (uiSize != nullptr)
        {
            const float s = processor.getUiScale();
            uiSize->setSelectedIndex (s < 0.87f ? 0 : s < 1.12f ? 1 : 2, false);
        }
        repaint();
    }

    void AdvancedPanel::resized()
    {
        const float selectorX = padding + labelWidth;
        const float selectorW = static_cast<float> (getWidth()) - selectorX - padding + 3.0f;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const float y = headerHeight + static_cast<float> (i) * rowHeight;
            const float inset = SegmentedSelector::insetFor (SegmentedSelector::Style::glass);
            rows[i].selector->setBounds (juce::Rectangle<float> (selectorX - inset, y + 4.0f - inset,
                                                                 selectorW, rowHeight - 8.0f + 2.0f * inset).toNearestInt());
            rows[i].selector->setPillGeometry (0.0f, 6.0f);
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

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const float y = headerHeight + static_cast<float> (i) * rowHeight;
            drawText (g, rows[i].label, { Weight::medium, 8.0f, 2.0f, rgb (170, 175, 180) },
                      padding, y + 0.5f * rowHeight - 4.0f, juce::Justification::left);
        }

        const float fy = headerHeight + static_cast<float> (rows.size()) * rowHeight + 10.0f;
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (padding, fy - 6.0f, r.getWidth() - 2.0f * padding, 1.0f));
        const auto info = "LATENCY " + juce::String (processor.getEngineLatency()) + " SAMPLES   "
                          + juce::String::fromUTF8 ("\xc2\xb7") + "   HEAT " + juce::String (HEAT_VERSION_STRING);
        drawText (g, info, { Weight::regular, 7.0f, 1.6f, rgb (120, 125, 130) },
                  padding, fy + 6.0f, juce::Justification::left);
    }
}
