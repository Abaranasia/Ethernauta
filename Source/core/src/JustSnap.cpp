#include <ethernauta/JustSnap.h>

namespace ethernauta {
namespace {

inline long long floorDiv(long long a, long long b) noexcept
{
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

constexpr double kDuplicateEpsilon = 1.0e-9;

// Finite but absurd cents would overflow the double -> long long conversions (undefined behaviour).
inline double clampSnapCents(double cents) noexcept
{
    return cents < -kMaxSnapCents ? -kMaxSnapCents : (cents > kMaxSnapCents ? kMaxSnapCents : cents);
}

} // namespace

void SnapCandidates::setJustScale() noexcept
{
    size_ = kNumScaleDegrees;
    for (int i = 0; i < kNumScaleDegrees; ++i) {
        cents_[i] = kJustTable[i].cents;
        ratio_[i] = kJustTable[i].ratio();
    }
}

int SnapCandidates::set(const double* cents, int count) noexcept
{
    size_ = 0;
    if (cents == nullptr || count <= 0) return 0;
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(cents[i])) continue;
        const double v = foldCents(cents[i]);
        // Insertion into a sorted array, skipping duplicates.
        int pos = size_;
        while (pos > 0 && cents_[pos - 1] > v) --pos;
        if (pos > 0 && v - cents_[pos - 1] < kDuplicateEpsilon) continue;
        if (pos < size_ && cents_[pos] - v < kDuplicateEpsilon) continue;
        if (size_ == kMaxCandidates) continue;
        for (int j = size_; j > pos; --j) cents_[j] = cents_[j - 1];
        cents_[pos] = v;
        ++size_;
    }
    for (int i = 0; i < size_; ++i) ratio_[i] = centsToRatio(cents_[i]);
    return size_;
}

double SnapCandidates::keyCents(long long key) const noexcept
{
    const long long oct = floorDiv(key, size_);
    return 1200.0 * static_cast<double>(oct) + cents_[key - oct * size_];
}

double SnapCandidates::keyRatio(long long key) const noexcept
{
    const long long oct = floorDiv(key, size_);
    return ratio_[key - oct * size_] * std::exp2(static_cast<double>(oct));
}

long long SnapCandidates::nearestKey(double cents) const noexcept
{
    cents = clampSnapCents(cents);
    const long long n = size_;
    const long long base = static_cast<long long>(std::floor(cents / 1200.0)) * n;
    // base - 1 is T.back() - 1200 and base + n is T.front() + 1200: the wraparound candidates.
    long long best = base - 1;
    double bestDist = std::fabs(cents - keyCents(best));
    for (long long k = base; k <= base + n; ++k) {
        const double d = std::fabs(cents - keyCents(k));
        if (d < bestDist) {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

long long selectSnapKey(const SnapCandidates& candidates, SnapState& state, double cents,
                        double hysteresisCents) noexcept
{
    cents = clampSnapCents(cents);
    const double hysteresis = hysteresisCents > 0.0 ? hysteresisCents : 0.0; // also catches NaN
    long long key = candidates.nearestKey(cents);
    if (state.valid && key != state.key) {
        const double keep = std::fabs(cents - candidates.keyCents(state.key));
        const double move = std::fabs(cents - candidates.keyCents(key));
        if (keep <= move + hysteresis) key = state.key;
    }
    state.valid = true;
    state.key = key;
    return key;
}

double snapCents(const SnapCandidates& candidates, SnapState& state, double cents, double strength,
                 double hysteresisCents) noexcept
{
    if (candidates.size() == 0 || !std::isfinite(cents)) return cents;
    cents = clampSnapCents(cents);
    const long long key = selectSnapKey(candidates, state, cents, hysteresisCents);
    const double s = strength > 0.0 ? (strength < 1.0 ? strength : 1.0) : 0.0;
    if (s <= 0.0) return cents;
    const double target = candidates.keyCents(key);
    return s >= 1.0 ? target : cents + s * (target - cents);
}

} // namespace ethernauta
