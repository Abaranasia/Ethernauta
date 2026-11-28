#include <ethernauta/TuningEngine.h>

namespace ethernauta {
namespace {

constexpr double kMinHz = 1.0e-3;
constexpr double kMaxHz = 1.0e7;

inline double sanitizeHz(double hz) noexcept
{
    if (!(hz > 0.0)) return kMinHz; // also catches NaN
    return hz > kMaxHz ? kMaxHz : hz;
}

template <typename T>
inline T clampTo(T v, T lo, T hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline long long floorDiv(long long a, long long b) noexcept
{
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

// Grid points are numbered along an endless ladder: Just key = octave * kNumScaleDegrees + degree,
// 12-TET key = semitone. keyCents() is the point's position above the root.
inline double keyCents(TuningMode mode, long long key) noexcept
{
    if (mode == TuningMode::EqualTempered) return 100.0 * static_cast<double>(key);
    const long long oct = floorDiv(key, kNumScaleDegrees);
    const long long idx = key - oct * kNumScaleDegrees;
    return 1200.0 * static_cast<double>(oct) + kJustTable[idx].cents;
}

inline long long nearestKey(TuningMode mode, double cents) noexcept
{
    if (mode == TuningMode::EqualTempered) return static_cast<long long>(std::llround(cents / 100.0));
    const long long base = static_cast<long long>(std::floor(cents / 1200.0)) * kNumScaleDegrees;
    // The neighbours across the octave boundaries (base - 1 and base + kNumScaleDegrees) are candidates too.
    long long best = base - 1;
    double bestDist = std::fabs(cents - keyCents(mode, best));
    for (long long k = base; k <= base + kNumScaleDegrees; ++k) {
        const double d = std::fabs(cents - keyCents(mode, k));
        if (d < bestDist) {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

inline double keyHz(TuningMode mode, long long key, int octaveOffset, double rootHz) noexcept
{
    if (mode == TuningMode::EqualTempered)
        return rootHz * std::exp2(static_cast<double>(key) / 12.0 + static_cast<double>(octaveOffset));
    const long long oct = floorDiv(key, kNumScaleDegrees);
    const long long idx = key - oct * kNumScaleDegrees;
    return rootHz * kJustTable[idx].ratio() * std::exp2(static_cast<double>(oct + octaveOffset));
}

} // namespace

void TuningEngine::reset() noexcept
{
    for (auto& m : memory_) m = Memory{};
}

void TuningEngine::setRoot(double midiNote, double cents) noexcept
{
    const double note = std::isfinite(midiNote) ? midiNote : 45.0;
    const double c = std::isfinite(cents) ? cents : 0.0;
    rootHz_ = pitchToHz(static_cast<float>(note), static_cast<float>(c));
}

double TuningEngine::targetHz(int voice, const VoiceTuning& tuning, double freeHz) noexcept
{
    const double f = sanitizeHz(freeHz);
    if (voice < 0 || voice >= kMaxVoices) return f;

    Memory& mem = memory_[voice];
    const int octave = clampTo(tuning.octave, kMinOctaveOffset, kMaxOctaveOffset);

    switch (tuning.mode) {
        case TuningMode::Degree: {
            mem.mode = TuningMode::Degree;
            const auto& d = kJustTable[clampTo(tuning.degree, 0, kNumJustDegrees - 1)];
            return rootHz_ * d.ratio() * std::exp2(static_cast<double>(octave));
        }
        case TuningMode::EqualTempered:
        case TuningMode::Just: {
            const double cents = ratioToCents(f / rootHz_);
            long long key = nearestKey(tuning.mode, cents);
            if (mem.mode == tuning.mode && key != mem.key) {
                // Stay on the current point unless the new one is clearly closer.
                const double keep = std::fabs(cents - keyCents(tuning.mode, mem.key));
                const double move = std::fabs(cents - keyCents(tuning.mode, key));
                if (keep <= move + kHysteresisCents) key = mem.key;
            }
            mem.mode = tuning.mode;
            mem.key = key;
            return keyHz(tuning.mode, key, octave, rootHz_);
        }
        case TuningMode::Free:
            break;
    }
    mem.mode = TuningMode::Free;
    return f;
}

} // namespace ethernauta
