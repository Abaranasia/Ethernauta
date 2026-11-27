#pragma once

namespace ethernauta {

// Stable order: the integer values are stored in plugin state as AudioParameterChoice indices.
// Append new waveforms at the end only.
enum class Waveform : int { Sine = 0, Saw = 1, Square = 2, Triangle = 3 };

inline constexpr int kNumWaveforms = 4;

inline const char* waveformName(Waveform w) noexcept
{
    switch (w) {
        case Waveform::Sine: return "Sine";
        case Waveform::Saw: return "Saw";
        case Waveform::Square: return "Square";
        case Waveform::Triangle: return "Triangle";
    }
    return "Sine";
}

// Clamping conversion for values arriving from hosts/state (never throws, never UB).
inline Waveform waveformFromIndex(int index) noexcept
{
    if (index < 0) index = 0;
    if (index >= kNumWaveforms) index = kNumWaveforms - 1;
    return static_cast<Waveform>(index);
}

} // namespace ethernauta
