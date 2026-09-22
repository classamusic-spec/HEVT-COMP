#include "UI/Header.h"
#include "UI/HeatTheme.h"
#include "UI/Icons.h"
#include "UI/Layout.h"
#include "PluginProcessor.h"

namespace heat::ui
{
    using namespace heat::ui::layout;

    namespace
    {
        const juce::Rectangle<float> headerArea { 500.0f, 36.0f, 800.0f, 92.0f };

        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }

        // Tracking of the reference preset name / tags (measured ink widths).
        float nameTracking()
        {
            static const float t = trackingForInkWidth ("Vocal Glue", Weight::regular, 13.0f, 104.0f);
            return t;
        }

        float tagsTracking()
        {
            static const float t = trackingForInkWidth (juce::String::fromUTF8 ("WARM \xc2\xb7 SMOOTH \xc2\xb7 PRESENT"),
                                                        Weight::regular, 9.0f, 228.3f);
            return t;
        }

        void drawGlowText (juce::Graphics& g, const juce::String& text, TextSpec spec, float centreX, float capTop,
                           juce::Colour glow, float maxInkWidth)
        {
            auto path = textPath (text, spec);
            auto ink = path.getBounds();
            if (ink.getWidth() > maxInkWidth && ink.getWidth() > 0.0f)
            {
                // Long names: tighten the tracking first, then scale.
                spec.tracking = std::max (0.0f, spec.tracking - (ink.getWidth() - maxInkWidth) / std::max (1, text.length() - 1));
                path = textPath (text, spec);
                ink = path.getBounds();
                if (ink.getWidth() > maxInkWidth)
                {
                    const float s = maxInkWidth / ink.getWidth();
                    spec.capHeight *= s;
                    path = textPath (text, spec);
                    ink = path.getBounds();
                    capTop += 0.5f * (1.0f - s) * 13.0f;
                }
            }
            path.applyTransform (juce::AffineTransform::translation (centreX - ink.getCentreX(), capTop + spec.capHeight));
            if (! glow.isTransparent())
            {
                g.setColour (glow.withAlpha (0.10f));
                g.strokePath (path, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (glow.withAlpha (0.18f));
                g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (spec.colour);
            g.fillPath (path);
        }
    }

    Header::Header (HeatAudioProcessor& p)
        : processor (p),
          prev ("Previous preset", [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down)
          {
              const auto c = r.getCentre();
              g.setColour ((over ? colours::cyanHot : rgb (223, 227, 230)).withAlpha (down ? 0.7f : 1.0f));
              g.strokePath (icons::chevron (c, 10.0f, 20.0f, true),
                            juce::PathStrokeType (2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
          }),
          next ("Next preset", [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down)
          {
              const auto c = r.getCentre();
              g.setColour ((over ? colours::cyanHot : rgb (223, 227, 230)).withAlpha (down ? 0.7f : 1.0f));
              g.strokePath (icons::chevron (c, 10.0f, 20.0f, false),
                            juce::PathStrokeType (2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
          }),
          name ("Preset browser", [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
          {
              if (over)
              {
                  g.setColour (juce::Colours::white.withAlpha (0.04f));
                  g.fillRoundedRectangle (r.reduced (2.0f), 8.0f);
              }
          }),
          heart ("Favourite", [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
          {
              auto& pm = processor.getPresetManager();
              const bool fav = pm.isFavourite (pm.getCurrentIndex());
              const auto path = icons::heart (r.getCentre(), 20.5f, 18.0f);
              if (fav)
              {
                  g.setColour (colours::amber.withAlpha (0.25f));
                  g.strokePath (path, juce::PathStrokeType (4.0f));
                  g.setColour (colours::amber);
                  g.fillPath (path);
              }
              g.setColour (fav ? colours::amberHot : (over ? colours::cyanHot : rgb (214, 219, 223)));
              g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
          }),
          gear ("Settings", [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
          {
              const auto path = icons::gear (r.getCentre(), 23.0f);
              const auto ink = settingsOpen ? colours::amberDeep : (over ? rgb (60, 64, 68) : rgb (30, 34, 38));
              g.setColour (juce::Colours::white.withAlpha (0.5f));
              g.strokePath (path, juce::PathStrokeType (2.0f), juce::AffineTransform::translation (0.0f, 0.8f));
              g.setColour (ink);
              g.strokePath (path, juce::PathStrokeType (2.0f));
          }),
          slotA ("A", [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
          {
              const bool active = processor.getActiveABSlot() == 0;
              drawTextInInkBox (g, "A", Weight::regular,
                                active ? rgb (27, 31, 35) : (over ? rgb (90, 94, 98) : colours::labelFaint),
                                r.getCentreX() - 5.85f, r.getCentreX() + 5.85f, r.getCentreY() - 6.7f, r.getCentreY() + 6.7f);
          }),
          slotB ("B", [this] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool)
          {
              const bool active = processor.getActiveABSlot() == 1;
              drawTextInInkBox (g, "B", Weight::regular,
                                active ? rgb (27, 31, 35) : (over ? rgb (90, 94, 98) : colours::labelFaint),
                                r.getCentreX() - 4.5f, r.getCentreX() + 4.5f, r.getCentreY() - 6.7f, r.getCentreY() + 6.7f);
          })
    {
        for (auto* b : { &prev, &next, &name, &heart, &gear, &slotA, &slotB })
            addAndMakeVisible (*b);

        prev.setTooltip ("Previous preset");
        next.setTooltip ("Next preset");
        name.setTooltip ("Browse presets");
        heart.setTooltip ("Add / remove favourite");
        gear.setTooltip ("Advanced settings");
        slotA.setTooltip ("Compare slot A  (right-click: copy)");
        slotB.setTooltip ("Compare slot B  (right-click: copy)");

        prev.onClick = [this] { processor.getPresetManager().loadPrevious(); };
        next.onClick = [this] { processor.getPresetManager().loadNext(); };
        name.onClick = [this] { showPresetMenu(); };
        heart.onClick = [this]
        {
            auto& pm = processor.getPresetManager();
            pm.toggleFavourite (pm.getCurrentIndex());
        };
        gear.onClick = [this] { if (onSettingsClicked) onSettingsClicked(); };

        auto abMenu = [this]
        {
            juce::PopupMenu m;
            m.addItem (1, "Copy A to B");
            m.addItem (2, "Copy B to A");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slotB),
                             [this] (int r)
                             {
                                 if (r == 1) processor.copyABSlot (0, 1);
                                 if (r == 2) processor.copyABSlot (1, 0);
                                 refresh();
                             });
        };
        slotA.onClick = [this] { processor.switchABSlot (0); refresh(); };
        slotB.onClick = [this] { processor.switchABSlot (1); refresh(); };
        slotA.onRightClick = abMenu;
        slotB.onRightClick = abMenu;

        processor.getPresetManager().addChangeListener (this);
        setBounds (headerArea.toNearestInt());
        refresh();
    }

    Header::~Header()
    {
        processor.getPresetManager().removeChangeListener (this);
    }

    void Header::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        refresh();
    }

    void Header::setSettingsOpen (bool open)
    {
        settingsOpen = open;
        gear.repaint();
    }

    void Header::refresh()
    {
        auto& pm = processor.getPresetManager();
        shownName = pm.getCurrentName();
        shownTags = pm.getCurrentTags();
        if (shownTags.isEmpty())
            shownTags = "USER";
        repaint();
    }

    juce::Point<float> Header::toLocal (float x, float y) const
    {
        return { x - headerArea.getX(), y - headerArea.getY() };
    }

    juce::Rectangle<float> Header::toLocal (juce::Rectangle<float> r) const
    {
        return r.translated (-headerArea.getX(), -headerArea.getY());
    }

    void Header::resized()
    {
        prev.setBounds (toLocal (presetPrev).toNearestInt());
        next.setBounds (toLocal (presetNext).toNearestInt());
        name.setBounds (toLocal (juce::Rectangle<float> (590.0f, 55.0f, 318.0f, 57.0f)).toNearestInt());
        heart.setBounds (toLocal (juce::Rectangle<float> (30.0f, 30.0f).withCentre (heartCentre)).toNearestInt());
        gear.setBounds (toLocal (juce::Rectangle<float> (34.0f, 34.0f).withCentre (gearCentre)).toNearestInt());
        slotA.setBounds (toLocal (juce::Rectangle<float> (30.0f, 30.0f).withCentre ({ 1220.85f, 84.0f })).toNearestInt());
        slotB.setBounds (toLocal (juce::Rectangle<float> (30.0f, 30.0f).withCentre ({ 1266.2f, 84.0f })).toNearestInt());
    }

    void Header::paint (juce::Graphics& g)
    {
        g.addTransform (juce::AffineTransform::translation (-headerArea.getX(), -headerArea.getY()));

        const auto outer = presetOuter;
        const float radius = 0.5f * outer.getHeight();

        // Shadow + machined rim.
        {
            juce::Path p;
            p.addRoundedRectangle (outer, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.28f), 8, { 0, 2 }).drawForPath (g, p);
            juce::ColourGradient rim (rgb (243, 245, 246), 0.0f, outer.getY(), rgb (231, 233, 235), 0.0f, outer.getBottom(), false);
            rim.addColour (0.5, rgb (150, 154, 158));
            g.setGradientFill (rim);
            g.fillPath (p);
            g.setColour (rgb (60, 64, 68, 0.8f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }

        // Dark glass body.
        const auto body = outer.reduced (3.2f);
        {
            juce::Path p;
            p.addRoundedRectangle (body, 0.5f * body.getHeight());
            juce::ColourGradient bg (rgb (30, 33, 35), 0.0f, body.getY(), rgb (20, 22, 23), 0.0f, body.getBottom(), false);
            bg.addColour (0.25, rgb (23, 25, 26));
            g.setGradientFill (bg);
            g.fillPath (p);

            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (p);

            // Arrow cells.
            for (auto cell : { presetPrev, presetNext })
            {
                juce::Path c;
                const bool left = cell.getX() < 700.0f;
                c.addRoundedRectangle (cell.getX(), cell.getY(), cell.getWidth(), cell.getHeight(), 14.0f, 14.0f,
                                       ! left, left, ! left, left);
                juce::ColourGradient cg (rgb (48, 52, 54), cell.getX(), cell.getY(), rgb (34, 37, 39), cell.getX(), cell.getBottom(), false);
                g.setGradientFill (cg);
                g.fillPath (c);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRect (juce::Rectangle<float> (left ? cell.getRight() : cell.getX() - 1.0f, cell.getY() + 2.0f, 1.0f, cell.getHeight() - 4.0f));
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRect (juce::Rectangle<float> (left ? cell.getRight() + 1.0f : cell.getX() - 2.0f, cell.getY() + 2.0f, 1.0f, cell.getHeight() - 4.0f));
            }

            // Top inner shadow and a faint bottom reflection.
            juce::ColourGradient top (juce::Colours::black.withAlpha (0.6f), 0.0f, body.getY(),
                                      juce::Colours::transparentBlack, 0.0f, body.getY() + 10.0f, false);
            g.setGradientFill (top);
            g.fillRect (body);
        }

        // Name + tags.
        const float nameCapTop = 67.4f;
        drawGlowText (g, shownName, { Weight::regular, 13.0f, nameTracking(), colours::presetName },
                      765.3f, nameCapTop, colours::cyan, 300.0f);
        drawGlowText (g, shownTags, { Weight::regular, 9.0f, tagsTracking(), colours::presetTags },
                      766.8f, 95.0f, juce::Colours::transparentBlack, 300.0f);

        // A / B underline: light base with a dark segment under the active slot.
        g.setColour (rgb (182, 185, 188));
        g.fillRect (juce::Rectangle<float> (1212.3f, abBaseline - 0.5f, 62.7f, 1.0f));
        const float activeCentre = processor.getActiveABSlot() == 0 ? 1220.8f : 1266.2f;
        g.setColour (rgb (27, 31, 35));
        g.fillRect (juce::Rectangle<float> (activeCentre - 8.5f, abBaseline - 1.0f, 17.0f, 2.0f));
    }

    void Header::showPresetMenu()
    {
        auto& pm = processor.getPresetManager();
        juce::PopupMenu menu;

        juce::PopupMenu favourites;
        for (int i = 0; i < pm.getNumPresets(); ++i)
            if (pm.isFavourite (i))
                favourites.addItem (i + 1, pm.getEntry (i).name, true, i == pm.getCurrentIndex());
        if (favourites.getNumItems() > 0)
        {
            menu.addSubMenu ("FAVOURITES", favourites);
            menu.addSeparator();
        }

        for (auto* category : heat::getPresetCategories())
        {
            juce::PopupMenu sub;
            for (int i = 0; i < pm.getNumPresets(); ++i)
                if (pm.getEntry (i).isFactory && pm.getEntry (i).category == category)
                    sub.addItem (i + 1, pm.getEntry (i).name, true, i == pm.getCurrentIndex());
            menu.addSubMenu (category, sub);
        }

        juce::PopupMenu user;
        for (int i = 0; i < pm.getNumPresets(); ++i)
            if (! pm.getEntry (i).isFactory)
                user.addItem (i + 1, pm.getEntry (i).name, true, i == pm.getCurrentIndex());
        menu.addSeparator();
        menu.addSubMenu ("USER", user, user.getNumItems() > 0);
        menu.addItem (10000, "Save Preset...");
        if (! pm.getEntry (pm.getCurrentIndex()).isFactory)
            menu.addItem (10001, "Delete \"" + pm.getCurrentName() + "\"");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name),
                            [this] (int result)
                            {
                                auto& presets = processor.getPresetManager();
                                if (result == 10000)
                                    showSavePresetDialog();
                                else if (result == 10001)
                                    presets.deleteUserPreset (presets.getCurrentIndex());
                                else if (result > 0)
                                    presets.loadPreset (result - 1);
                            });
    }

    void Header::showSavePresetDialog()
    {
        auto* window = new juce::AlertWindow ("Save Preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
        window->setLookAndFeel (&getLookAndFeel());
        window->addTextEditor ("name", processor.getPresetManager().getCurrentName());
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<Header> safe (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
        {
            if (safe != nullptr && result == 1)
                safe->processor.getPresetManager().saveUserPreset (window->getTextEditorContents ("name"));
        }), true);
    }
}
