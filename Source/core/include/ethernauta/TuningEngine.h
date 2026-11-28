#pragma once
#include <ethernauta/Tuning.h>

namespace ethernauta {

// Order matches the plugin's "v{n}_tuning" choice indices; do not reorder.
enum class TuningMode { Free = 0, EqualTempered = 1, Just = 2, Degree = 3 };
inline constexpr int kNumTuningModes = 4;

inline constexpr int kMinOctaveOffset = -3;
inline constexpr int kMaxOctaveOffset = 3;

struct VoiceTuning {
    TuningMode mode = TuningMode::Free;
    int degree = 0; // index into kJustTable (Degree mode only)
    int octave = 0; // octaves added to the tuned result (all modes except Free)
};

// Computes each voice's target frequency from ONE shared root, never neighbour to neighbour, so
// intervals cannot accumulate comma drift. Allocation-free; call from the audio thread only.
//
//   Free           the voice's own frequency, unchanged
//   EqualTempered  nearest 12-TET semitone relative to the root, octave kept
//   Just           nearest octave-reduced just interval relative to the root, octave kept
//   Degree         root * ratio(degree) * 2^octave; ignores the voice's own frequency
//
// Snapping has hysteresis: a voice leaves its current grid point only once another point is more than
// kHysteresisCents closer, so jitter around a midpoint cannot flap the target. The memory is per
// voice and cleared when the voice leaves the snapping modes.
class TuningEngine {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr double kHysteresisCents = 3.0;

    void reset() noexcept;
    // Root as a MIDI note plus cents (pitchToHz conventions). Non-finite input is replaced by A2.
    void setRoot(double midiNote, double cents = 0.0) noexcept;
    double rootHz() const noexcept { return rootHz_; }

    // `freeHz` is the voice's own pitch. The result is always finite and positive.
    double targetHz(int voice, const VoiceTuning& tuning, double freeHz) noexcept;

private:
    struct Memory {
        TuningMode mode = TuningMode::Free;
        long long key = 0; // grid point index, valid when mode is a snapping mode
    };

    double rootHz_ = 110.0;
    Memory memory_[kMaxVoices];
};

} // namespace ethernauta
