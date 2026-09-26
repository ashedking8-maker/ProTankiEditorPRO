#pragma once
#include <array>
#include <cstddef>

// Pure, platform-independent state machine for the three independent native
// pickers. Cancelling a picker never changes this state. A successful load or
// an explicit "do not show again" are the only ways to suppress an explanation.
enum class GuideTarget : std::size_t { Map = 0, Library = 1, Tester = 2 };

class GuidanceState {
public:
    bool NeedsExplanation(GuideTarget target) const { return !seen_[Index(target)]; }
    void RecordSuccess(GuideTarget target) { seen_[Index(target)] = true; }
    void Suppress(GuideTarget target) { seen_[Index(target)] = true; }
    void Restore(GuideTarget target, bool seen) { seen_[Index(target)] = seen; }
    bool Seen(GuideTarget target) const { return seen_[Index(target)]; }
private:
    static constexpr std::size_t Index(GuideTarget target) { return static_cast<std::size_t>(target); }
    std::array<bool,3> seen_{};
};
