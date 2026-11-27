// Catch2 runner for the JUCE-dependent plugin tests. JUCE needs its subsystems initialised once
// before an AudioProcessor/editor is created; a run-scoped listener does that.
#include <juce_events/juce_events.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <memory>

namespace {
class JuceRuntimeListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo&) override
    {
        juce_ = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    }
    void testRunEnded(const Catch::TestRunStats&) override { juce_.reset(); }

private:
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juce_;
};
} // namespace

CATCH_REGISTER_LISTENER(JuceRuntimeListener)
