#pragma once
#include <ethernauta/Tuning.h>

namespace ethernauta {

// A choir is a group of kChoirSize voices tuned as a triad from the ONE shared root (never neighbour
// to neighbour, so intervals cannot accumulate comma drift). Voice v is member (v % kChoirSize):
// 0 = root, 1 = third, 2 = fifth. Voices 1-3 form choir 1 and voices 4-6 choir 2 (zero-based 0-2, 3-5).
inline constexpr int kChoirSize = 3;

// The chord a choir sings. The order is a stable contract (stored in presets and automation):
// entries may only be appended.
enum class ChoirChord : int { Major = 0, Minor = 1, SeptimalMinor = 2, Supermajor = 3 };
inline constexpr int kNumChoirChords = 4;

namespace detail {
struct ChoirRatio {
    int num;
    int den;
};
// [chord][member]: exact just ratios above the root.
inline constexpr ChoirRatio kChoirTable[kNumChoirChords][kChoirSize] = {
    {{1, 1}, {5, 4}, {3, 2}}, // Major          1  5/4 3/2
    {{1, 1}, {6, 5}, {3, 2}}, // Minor          1  6/5 3/2
    {{1, 1}, {7, 6}, {3, 2}}, // SeptimalMinor  1  7/6 3/2
    {{1, 1}, {9, 7}, {3, 2}}, // Supermajor     1  9/7 3/2
};
constexpr const ChoirRatio& choirEntry(ChoirChord chord, int member) noexcept
{
    const int c = static_cast<int>(chord);
    // Sanitising: an out-of-range chord counts as Major, an out-of-range member as the root (1/1).
    const int ci = (c >= 0 && c < kNumChoirChords) ? c : 0;
    const int mi = (member >= 0 && member < kChoirSize) ? member : 0;
    return kChoirTable[ci][mi];
}
} // namespace detail

// Ratio of a member above the root, as an exact num/den division. Out-of-range chord -> Major,
// out-of-range member -> member 0 (ratio 1). Pure, allocation-free, noexcept.
inline constexpr double choirRatio(ChoirChord chord, int member) noexcept
{
    const auto& e = detail::choirEntry(chord, member);
    return static_cast<double>(e.num) / static_cast<double>(e.den);
}

// The same interval in cents (1200 * log2(ratio)); same sanitising as choirRatio().
inline double choirCents(ChoirChord chord, int member) noexcept { return ratioToCents(choirRatio(chord, member)); }

} // namespace ethernauta
