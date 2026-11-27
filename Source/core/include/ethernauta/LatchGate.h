#pragma once

namespace ethernauta {

// Click-free on/off gate. setOpen(true) ramps the output 0 -> 1 over the attack time, setOpen(false)
// ramps it 1 -> 0 over the release time, both with a smoothstep curve (zero slope at the ends), and
// reversing mid-ramp is continuous. The output is exactly 0.0 when fully closed and exactly 1.0 when
// fully open. Allocation-free.
class LatchGate {
public:
    void prepare(double sampleRate, double attackSeconds, double releaseSeconds) noexcept;
    void setOpen(bool open) noexcept { open_ = open; }
    bool isOpen() const noexcept { return open_; }
    // True when closed AND fully released: the output is exactly 0 and next() would stay 0.
    bool isIdle() const noexcept { return !open_ && progress_ <= 0.0f; }
    void reset() noexcept { progress_ = 0.0f; }

    float next() noexcept;

private:
    float attackStep_ = 1.0f / 960.0f;
    float releaseStep_ = 1.0f / 14400.0f;
    float progress_ = 0.0f; // linear ramp position in [0, 1]
    bool open_ = false;
};

} // namespace ethernauta
