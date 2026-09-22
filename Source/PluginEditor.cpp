#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Core/Constants.h"
#include "Graphics/SilverSurface.h"
#include "UI/HeatTheme.h"
#include "UI/Layout.h"

namespace heat::ui
{
    using namespace heat::ui::layout;

    namespace
    {
        juce::RangedAudioParameter& param (HeatAudioProcessor& p, const char* id)
        {
            auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
            jassert (r != nullptr);
            return *r;
        }

        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }
    }

    // Transparent layer that closes the advanced panel on an outside click.
    class MainPanel::Scrim : public juce::Component
    {
    public:
        std::function<void()> onClick;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };

    // ------------------------------------------------------------------------------
    void ValueBubble::show (const juce::String& t, const juce::String& v, const juce::String& d,
                            juce::Point<float> anchor, float anchorRadius)
    {
        title = t;
        value = v;
        detail = d;
        const float w = detail.isEmpty() ? 150.0f : 250.0f;
        const float h = detail.isEmpty() ? 44.0f : 62.0f;
        auto r = juce::Rectangle<float> (w, h).withCentre ({ anchor.x, anchor.y - anchorRadius - 0.5f * h - 10.0f });
        r = r.constrainedWithin ({ 8.0f, 24.0f, canvasWidth - 16.0f, faceBottom - 32.0f });
        setBounds (r.toNearestInt());
        setVisible (true);
        repaint();
    }

    void ValueBubble::paint (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 10.0f);
        juce::ColourGradient bg (rgb (34, 37, 40, 0.97f), 0.0f, r.getY(), rgb (20, 22, 24, 0.97f), 0.0f, r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (rgb (160, 164, 168, 0.55f));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);

        drawText (g, title, { Weight::medium, 7.5f, 2.2f, rgb (150, 155, 160) }, r.getCentreX(), r.getY() + 9.0f);
        drawText (g, value, { Weight::regular, 12.0f, 0.8f, colours::amberText }, r.getCentreX(), r.getY() + 22.0f);
        if (detail.isNotEmpty())
            drawText (g, detail, { Weight::regular, 8.0f, 0.9f, rgb (143, 212, 255) }, r.getCentreX(), r.getY() + 44.0f);
    }

    // ------------------------------------------------------------------------------
    MainPanel::MainPanel (HeatAudioProcessor& p)
        : processor (p),
          header (p),
          input (param (p, ids::input), &p.getUndoManager(), HeatKnob::Style::large, HeatKnob::Arc::cyanFromStart, "INPUT"),
          output (param (p, ids::output), &p.getUndoManager(), HeatKnob::Style::large, HeatKnob::Arc::cyanFromEnd, "OUTPUT"),
          attack (param (p, ids::attack), &p.getUndoManager(), HeatKnob::Style::medium, HeatKnob::Arc::cyanFromStart, "ATTACK"),
          release (param (p, ids::release), &p.getUndoManager(), HeatKnob::Style::medium, HeatKnob::Arc::cyanFromStart, "RELEASE"),
          compress (param (p, ids::compress), &p.getUndoManager(), HeatKnob::Style::compress, HeatKnob::Arc::amberHeat, "COMPRESS"),
          tube (param (p, ids::tube), &p.getUndoManager(), HeatKnob::Style::small, HeatKnob::Arc::none, "TUBE"),
          iron (param (p, ids::iron), &p.getUndoManager(), HeatKnob::Style::small, HeatKnob::Arc::none, "IRON"),
          mix (param (p, ids::mix), &p.getUndoManager(), HeatKnob::Style::small, HeatKnob::Arc::none, "MIX"),
          hpf (param (p, ids::hpf), &p.getUndoManager(), HeatKnob::Style::small, HeatKnob::Arc::none, "HPF"),
          mode (heat::modeChoices, SegmentedSelector::Style::panel, "MODE"),
          detector (heat::detectorChoices, SegmentedSelector::Style::panel, "DETECTOR"),
          scrim (std::make_unique<Scrim>()),
          advanced (p)
    {
        setSize (static_cast<int> (canvasWidth), static_cast<int> (canvasHeight));
        setOpaque (true);

        addAndMakeVisible (meter);
        meter.setInstrumentBounds (meterOuter);
        meter.setPeakHoldEnabled (processor.getPeakHold());

        const std::pair<HeatKnob*, juce::Point<float>> knobs[] = {
            { &input, inputCentre }, { &output, outputCentre }, { &attack, attackCentre }, { &release, releaseCentre },
            { &compress, compressCentre }, { &tube, tubeCentre }, { &iron, ironCentre }, { &mix, mixCentre }, { &hpf, hpfCentre }
        };
        for (auto& [knob, centre] : knobs)
        {
            addAndMakeVisible (*knob);
            knob->setCentre (centre);
            knob->onInteraction = [this] (HeatKnob& k, bool active) { knobInteraction (k, active); };
        }
        compress.extraInfo = [this] { return compressDetail(); };

        addAndMakeVisible (header);

        const float modeInset = SegmentedSelector::insetFor (SegmentedSelector::Style::panel);
        mode.setBounds (juce::Rectangle<float> (modeX0 - modeInset, buttonTop - modeInset,
                                                3.0f * modeButtonW + 2.0f * modeGap + 2.0f * modeInset,
                                                buttonHeight + 2.0f * modeInset).toNearestInt());
        mode.setPillGeometry (modeButtonW, modeGap);
        mode.bindToParameter (param (p, ids::mode), &p.getUndoManager());
        mode.setTooltips ({ "CLEAN - transparent, predictable compression",
                            "WARM - smooth, program-dependent leveling with gentle colour",
                            "DRIVE - fast, dense, energetic compression with saturation" });
        addAndMakeVisible (mode);

        detector.setBounds (juce::Rectangle<float> (detX0 - modeInset, buttonTop - modeInset,
                                                    3.0f * detButtonW + 2.0f * detGap + 2.0f * modeInset,
                                                    buttonHeight + 2.0f * modeInset).toNearestInt());
        detector.setPillGeometry (detButtonW, detGap);
        detector.bindToParameter (param (p, ids::detector), &p.getUndoManager());
        detector.setTooltips ({ "PEAK - fast, transient-accurate detection",
                                "RMS - true power detection, smooth and energy-oriented",
                                "OPTICAL - program-dependent light-cell leveling" });
        addAndMakeVisible (detector);

        input.setTooltip ("INPUT drive into the compressor and colour stages");
        output.setTooltip ("OUTPUT level");
        attack.setTooltip ("ATTACK: fast to slow");
        release.setTooltip ("RELEASE: fast to slow");
        compress.setTooltip ("COMPRESS: less to more");
        tube.setTooltip ("TUBE colour");
        iron.setTooltip ("IRON weight");
        mix.setTooltip ("MIX: dry to wet (parallel compression)");
        hpf.setTooltip ("Sidechain high-pass (detector only)");

        header.onSettingsClicked = [this] { toggleAdvanced(); };

        scrim->onClick = [this] { toggleAdvanced(); };
        scrim->setBounds (getLocalBounds());
        addChildComponent (*scrim);
        addChildComponent (advanced);
        advanced.onClose = [this] { toggleAdvanced(); };
        advanced.onPeakHold = [this] (bool b) { meter.setPeakHoldEnabled (b); };
        advanced.onUiScale = [this] (float s) { if (onUiScaleRequest) onUiScaleRequest (s); };
        advanced.onGpuMeter = [this] (bool b) { if (onGpuMeterRequest) onGpuMeterRequest (b); };

        addChildComponent (bubble);

        setFocusContainerType (FocusContainerType::keyboardFocusContainer);
        setTitle ("HEAT Analog Dynamics Processor");
    }

    MainPanel::~MainPanel() = default;

    void MainPanel::setGpuHole (bool enabled)
    {
        if (gpuHole == enabled)
            return;
        gpuHole = enabled;
        meter.setGpuMode (enabled);
        repaint (meter.getBounds());
    }

    void MainPanel::paint (juce::Graphics& g)
    {
        if (gpuHole)
        {
            // Leave the meter glass transparent: the GPU layer shows through.
            juce::Path hole;
            hole.addRectangle (getLocalBounds().toFloat());
            hole.addRoundedRectangle (GainReductionDisplay::glassBounds(), GainReductionDisplay::glassRadius());
            hole.setUsingNonZeroWinding (false);
            g.reduceClipRegion (hole);
        }

        const float scale = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        if (chassis.isNull() || std::abs (chassisScale - scale) > 0.01f)
        {
            chassisScale = scale;
            chassis = juce::Image (juce::Image::RGB, juce::roundToInt (canvasWidth * scale), juce::roundToInt (canvasHeight * scale), false);
            juce::Graphics cg (chassis);
            cg.addTransform (juce::AffineTransform::scale (scale));
            surface::paintChassis (cg);
        }
        g.drawImageTransformed (chassis, juce::AffineTransform::scale (1.0f / chassisScale));
    }

    void MainPanel::toggleAdvanced()
    {
        const bool open = ! advanced.isVisible();
        advanced.syncFromProcessor();
        scrim->setVisible (open);
        advanced.setVisible (open);
        header.setSettingsOpen (open);
        if (open)
        {
            scrim->toFront (false);
            advanced.toFront (true);
        }
    }

    juce::String MainPanel::compressDetail() const
    {
        auto& t = processor.getTelemetry();
        const float ratio = t.getRatio();
        juce::String r = ratio > 50.0f ? juce::String::fromUTF8 ("\xe2\x88\x9e") : juce::String (ratio, ratio < 10.0f ? 1 : 0);
        return "THR " + juce::String (t.getThresholdDb(), 1) + " dB   RATIO " + r + ":1   GR "
               + juce::String (std::min (t.getCurrentGrDb (0), t.getCurrentGrDb (1)), 1) + " dB";
    }

    void MainPanel::knobInteraction (HeatKnob& knob, bool active)
    {
        if (active)
        {
            activeKnob = &knob;
            updateBubble();
        }
        else if (activeKnob == &knob)
        {
            activeKnob = nullptr;
            bubble.setVisible (false);
        }
    }

    void MainPanel::updateBubble()
    {
        if (activeKnob == nullptr)
            return;
        auto& k = *activeKnob;
        const float radius = k.getFaceRadius() + (&k == &compress ? 44.0f : &k == &attack || &k == &release ? 20.0f : 20.0f);
        bubble.show (k.getDisplayName(), k.getValueText(), &k == &compress ? compressDetail() : juce::String(),
                     k.getCentreInParent(), radius);
    }

    void MainPanel::setPreviewGainReduction (float db)
    {
        meter.setImmediate (db, db, db);
        compress.setHeat (juce::jlimit (0.0f, 1.0f, -db / 3.0f));
    }

    void MainPanel::onFrame (double dt)
    {
        auto& t = processor.getTelemetry();
        const auto blocks = t.getBlockCount();
        float left, right;
        if (blocks != lastBlocks)
        {
            left = t.consumeDeepestGrDb (0);
            right = t.consumeDeepestGrDb (1);
            lastBlocks = blocks;
        }
        else
        {
            left = t.getCurrentGrDb (0);
            right = t.getCurrentGrDb (1);
        }
        meter.pushFrame (left, right, dt);
        compress.setHeat (juce::jlimit (0.0f, 1.0f, -std::min (meter.getDisplayedDb (0), meter.getDisplayedDb (1)) / 3.0f));

        if (activeKnob == &compress && bubble.isVisible())
            updateBubble();
        if (advanced.isVisible())
            advanced.updateLive (dt);
    }
}

// ==================================================================================

HeatAudioProcessorEditor::HeatAudioProcessorEditor (HeatAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      panel (p),
      vblank (this, [this] (double timestamp)
      {
          const double dt = lastFrameTime > 0.0 ? juce::jlimit (0.0, 0.1, timestamp - lastFrameTime) : 1.0 / 60.0;
          lastFrameTime = timestamp;
          panel.onFrame (dt);
      })
{
    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (panel);
    panel.onUiScaleRequest = [this] (float s) { applyScale (s); };
    panel.onGpuMeterRequest = [this] (bool b) { setGpuMeterEnabled (b); };
    panel.getMeter().onGpuFrame = [this]
    {
        if (gpuRenderer == nullptr)
            return;
        auto& m = panel.getMeter();
        gpuRenderer->setState (m.getDisplayedDb (0), m.getDisplayedDb (1), m.getPeakDb());
        glContext.triggerRepaint();
    };

    constrainer.setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);
    constrainer.setSizeLimits (designWidth / 2, designHeight / 2, designWidth * 5 / 4, designHeight * 5 / 4);
    setConstrainer (&constrainer);
    setResizable (true, true);

    setWantsKeyboardFocus (true);
    applyScale (processor.getUiScale());
    setGpuMeterEnabled (processor.getGpuMeter());
}

HeatAudioProcessorEditor::~HeatAudioProcessorEditor()
{
    setGpuMeterEnabled (false);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void HeatAudioProcessorEditor::setGpuMeterEnabled (bool enabled)
{
    if (enabled == (gpuRenderer != nullptr))
        return;

    if (! enabled)
    {
        gpuActive = false;
        panel.setGpuHole (false);
        glContext.detach();
        gpuRenderer.reset();
        return;
    }

    gpuRenderer = std::make_unique<heat::ui::GpuMeterRenderer> (glContext);
    juce::Component::SafePointer<HeatAudioProcessorEditor> safe (this);
    gpuRenderer->onReady = [safe] (bool ok)
    {
        juce::MessageManager::callAsync ([safe, ok] { if (safe != nullptr) safe->gpuReady (ok); });
    };
    gpuRenderer->setPlacement (static_cast<float> (getWidth()) / static_cast<float> (designWidth), getWidth(), getHeight());
    glContext.setRenderer (gpuRenderer.get());
    glContext.setComponentPaintingEnabled (true);
    glContext.setContinuousRepainting (false);
    glContext.attachTo (*this);
}

void HeatAudioProcessorEditor::gpuReady (bool ok)
{
    if (gpuRenderer == nullptr)
        return;
    if (! ok)
    {
        // No usable OpenGL here: stay on the CPU meter.
        setGpuMeterEnabled (false);
        return;
    }
    gpuActive = true;
    panel.setGpuHole (true);
    repaint();
}

void HeatAudioProcessorEditor::applyScale (float scale)
{
    scale = juce::jlimit (0.5f, 1.25f, scale);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));
}

void HeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (panel.hasGpuHole())
        g.excludeClipRegion (getLocalArea (&panel, heat::ui::GainReductionDisplay::glassBounds()).toNearestInt());
    g.fillAll (juce::Colour (0xff57585a));
}

void HeatAudioProcessorEditor::resized()
{
    const float scale = static_cast<float> (getWidth()) / static_cast<float> (designWidth);
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, designWidth, designHeight);
    processor.setUiScale (scale);
    if (gpuRenderer != nullptr)
    {
        gpuRenderer->setPlacement (scale, getWidth(), getHeight());
        glContext.triggerRepaint();
    }
}

bool HeatAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z')
    {
        if (mods.isCommandDown() && mods.isShiftDown())
            return processor.getUndoManager().redo();
        if (mods.isCommandDown())
            return processor.getUndoManager().undo();
    }
    if ((key.getKeyCode() == 'Y' || key.getKeyCode() == 'y') && mods.isCommandDown())
        return processor.getUndoManager().redo();
    return false;
}
