#pragma once
#include <ethernauta/Tuning.h>

namespace ethernauta {

inline constexpr double kDefaultSnapHysteresisCents = 3.0;

// Cents beyond this magnitude (about 83 million octaves) are clamped before grid search so the
// double -> long long conversions can never overflow. Far outside any audible or parameter range.
inline constexpr double kMaxSnapCents = 1.0e9;

// Octave-reduced snap candidates, in cents above the shared root. Fixed capacity, so configuring and
// querying never allocate. The set repeats every octave: grid points are numbered along an endless
// ladder, key = octave * size() + index, so the neighbours across an octave boundary
// (T.back() - 1200 below, T.front() + 1200 above) are ordinary keys, not special cases.
class SnapCandidates {
public:
    static constexpr int kMaxCandidates = 32;

    // The 16 octave-reduced degrees of kJustTable, with their exact ratios.
    void setJustScale() noexcept;

    // Folds every finite value into [0, 1200), sorts, removes duplicates and keeps at most
    // kMaxCandidates. Returns the resulting size. Ratios are derived from the cents.
    int set(const double* cents, int count) noexcept;

    int size() const noexcept { return size_; }
    double cents(int i) const noexcept { return cents_[i]; }
    double ratio(int i) const noexcept { return ratio_[i]; }

    // Position of a grid point above the root. Requires size() > 0.
    double keyCents(long long key) const noexcept;
    // Frequency ratio of a grid point to the root (exact for the table scale). Requires size() > 0.
    double keyRatio(long long key) const noexcept;
    // The grid point nearest to `cents`, considering the wraparound neighbours. Requires size() > 0.
    long long nearestKey(double cents) const noexcept;

private:
    double cents_[kMaxCandidates] = {};
    double ratio_[kMaxCandidates] = {};
    int size_ = 0;
};

// Per-voice memory of the grid point currently held.
struct SnapState {
    bool valid = false;
    long long key = 0;
    void reset() noexcept { *this = SnapState{}; }
};

// Picks the grid point for `cents`. The remembered point is kept unless the nearest one is closer by
// MORE than `hysteresisCents` (negative or NaN counts as 0), so jitter around a midpoint cannot flap.
// Updates `state`. Requires candidates.size() > 0.
long long selectSnapKey(const SnapCandidates& candidates, SnapState& state, double cents,
                        double hysteresisCents) noexcept;

// Snaps a pitch given in cents above the root. `strength` is clamped to [0, 1] (NaN counts as 0):
// 0 returns `cents` unchanged, 1 returns the grid point, in between is linear in cents. An empty
// candidate set returns `cents` unchanged. Allocation-free and noexcept.
double snapCents(const SnapCandidates& candidates, SnapState& state, double cents, double strength,
                 double hysteresisCents) noexcept;

} // namespace ethernauta
