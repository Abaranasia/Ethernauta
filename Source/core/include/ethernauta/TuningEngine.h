#pragma once
#include <ethernauta/JustSnap.h>
#include <ethernauta/Tuning.h>

#include <atomic>

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
    // Just mode only. 1 = full snap (default, the original behaviour), 0 = the voice's own pitch,
    // linear in cents in between. Clamped to [0, 1]; NaN counts as 0.
    double snapStrength = 1.0;
    // Just mode only. A voice leaves its grid point only once another is closer by more than this.
    double snapHysteresisCents = kDefaultSnapHysteresisCents;
};

// Computes each voice's target frequency from ONE shared root, never neighbour to neighbour, so
// intervals cannot accumulate comma drift. Allocation-free; call from the audio thread only
// (setSnapCandidates() is the one exception: message thread, see below).
//
//   Free           the voice's own frequency, unchanged
//   EqualTempered  nearest 12-TET semitone relative to the root, octave kept
//   Just           nearest snap candidate (default: the octave-reduced just scale) relative to the
//                  root, octave kept, pulled with snapStrength; see JustSnap.h
//   Degree         root * ratio(degree) * 2^octave; ignores the voice's own frequency
//
// Snapping has hysteresis: a voice leaves its current grid point only once another point is more than
// the hysteresis closer (kHysteresisCents for 12-TET; VoiceTuning::snapHysteresisCents for Just),
// so jitter around a midpoint cannot flap the target. The memory is per voice and cleared when the voice leaves the snapping modes.
class TuningEngine {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr double kHysteresisCents = 3.0; // 12-TET only; independent of the Just default

    void reset() noexcept;
    // Root as a MIDI note plus cents (pitchToHz conventions). Non-finite input is replaced by A2.
    void setRoot(double midiNote, double cents = 0.0) noexcept;
    // Publishes the Just-mode candidate set (default: the just scale). Wait-free, allocation-free.
    //
    // Thread contract (exactly one thread in each role):
    //   message thread  setSnapCandidates()                         (single writer)
    //   audio thread    targetHz(), reset(), snapCandidates()       (single reader; owns memory_)
    // setRoot() and rootHz() are plain, unsynchronised state and stay audio-thread (or pre-start) only.
    // The set takes effect at the audio thread's next targetHz()/reset() call: that call adopts the
    // latest published set and clears every voice's snap memory, once per adopted set (sets published
    // twice before the audio thread runs are coalesced, only the last is adopted). The setter never
    // touches memory_. With a single thread doing both, "set, then next targetHz()" behaves as before.
    void setSnapCandidates(const SnapCandidates& candidates) noexcept;
    // The set the audio thread currently uses (the last adopted one). Audio thread only; it does not
    // yet reflect a set published but not adopted.
    const SnapCandidates& snapCandidates() const noexcept { return slots_[readIdx_]; }
    double rootHz() const noexcept { return rootHz_; }

    // `freeHz` is the voice's own pitch. The result is always finite and positive.
    double targetHz(int voice, const VoiceTuning& tuning, double freeHz) noexcept;

private:
    struct Memory {
        TuningMode mode = TuningMode::Free;
        long long key = 0; // grid point index, valid when mode is a snapping mode
    };

    // Adopts a pending published set, if any, and clears all voice memory. Audio thread only.
    void adoptPending() noexcept;

    // Triple buffer. Of the three slots, one is owned by the reader (readIdx_), one by the writer
    // (writeIdx_), and one sits in the mailbox (mailbox_ low bits). Each side writes only to the slot
    // it owns, so a slot is never read and written at once and no copy is ever torn. Chosen over a
    // seqlock because the reader must never retry or see a half-written set, and over a mutex or
    // spinlock because the audio side must stay wait-free; the cost is two spare 530-byte slots.
    // A set is never copied on the audio thread: adopting is an index swap.
    static constexpr int kDirty = 4; // mailbox_ flag: the mailbox slot holds an unadopted publication
    static constexpr int kIdxMask = 3;
    static_assert(std::atomic<int>::is_always_lock_free, "the handoff must be lock-free");

    static SnapCandidates defaultCandidates() noexcept
    {
        SnapCandidates c;
        c.setJustScale();
        return c;
    }

    double rootHz_ = 110.0;
    SnapCandidates slots_[3] = {defaultCandidates(), defaultCandidates(), defaultCandidates()};
    int readIdx_ = 0;  // audio thread only
    int writeIdx_ = 1; // message thread only
    std::atomic<int> mailbox_{2}; // slot index, plus kDirty when freshly published
    Memory memory_[kMaxVoices];   // audio thread only
};

} // namespace ethernauta
