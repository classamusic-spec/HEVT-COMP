// heat_snapshot — renders the HEAT editor headlessly to PNG for visual
// comparison with design/reference/HEAT_LOCKED_REFERENCE.png.
//
//   heat_snapshot <out.png> [scale=1.0] [grDb=-3.66] [state=reference|advanced|clean]

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
