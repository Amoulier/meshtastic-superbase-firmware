#pragma once

#include <cstdint>

// Mechanical, active-low switch state. IRQs preserve short taps, but sampled levels also
// work when an interrupt cannot be allocated. All timing uses wrap-safe elapsed durations.
class SuperbaseButtonState
{
  public:
    enum class Event : uint8_t { None, Press, ShortRelease, LongPress };
    static constexpr uint32_t debounceMs = 10;
    static constexpr uint32_t longMs = 500;

    Event poll(bool down, bool edge, uint32_t edgeAt, uint32_t now, bool select)
    {
        if (!active) {
            if (!edge) {
                if (!down) {
                    samplingPress = false;
                    return Event::None;
                }
                if (!samplingPress) {
                    samplingPress = true;
                    sampledAt = now;
                    return Event::None;
                }
                if (uint32_t(now - sampledAt) < debounceMs)
                    return Event::None;
                edgeAt = sampledAt;
            }
            samplingPress = false;
            pressedAt = edgeAt;
            longSent = false;
            releasing = false;
            active = down;
            if (!select)
                return Event::Press;
            // A released tap must not disappear just because the cooperative loop was late.
            if (!down)
                return uint32_t(now - pressedAt) < longMs ? Event::ShortRelease : Event::None;
        }

        if (!down) {
            if (!releasing) {
                releasing = true;
                releasedAt = now;
            }
            if (uint32_t(now - releasedAt) < debounceMs)
                return Event::None;
            active = false;
            releasing = false;
            return select && !longSent && uint32_t(releasedAt - pressedAt) < longMs ? Event::ShortRelease : Event::None;
        }

        releasing = false;
        const uint32_t repeatMs = select ? 300 : 150;
        if (uint32_t(now - pressedAt) >= longMs && (!longSent || uint32_t(now - repeatedAt) >= repeatMs)) {
            longSent = true;
            repeatedAt = now;
            return select ? Event::LongPress : Event::Press;
        }
        return Event::None;
    }

    bool needsFastPoll() const { return active || samplingPress; }

  private:
    bool active = false;
    bool longSent = false;
    bool releasing = false;
    bool samplingPress = false;
    uint32_t pressedAt = 0;
    uint32_t repeatedAt = 0;
    uint32_t releasedAt = 0;
    uint32_t sampledAt = 0;
};
