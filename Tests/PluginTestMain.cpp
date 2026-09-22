#include "TestFramework.h"

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    // Plugin tests need a message manager (preset change broadcasts, editor
    // lifecycle tests).
    juce::ScopedJuceInitialiser_GUI juceInit;
    const int result = heat::test::runAll (argc > 1 ? argv[1] : nullptr);
    juce::DeletedAtShutdown::deleteAll();
    return result;
}
