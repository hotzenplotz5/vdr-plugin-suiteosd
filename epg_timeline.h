#ifndef VDR_SUITEOSD_EPG_TIMELINE_H
#define VDR_SUITEOSD_EPG_TIMELINE_H

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace suiteosd {

// Native VDR EPG snapshots. No VDR pointers, HTTP requests or duplicated
// programme database are retained between key events.
struct TimelineEvent {
    uint32_t id = 0;
    time_t start = 0;
    time_t end = 0;
    std::string title;
    std::string subtitle;
    std::string description;
};
struct TimelineChannel {
    int number = 0;
    std::string name;
    std::vector<TimelineEvent> events;
};

struct Timeline {
    // Webfrontend's 24h page model, 30min anchoring, now context and
    // chronological proportional tiles. The TV viewport shows a zoomed
    // four-hour portion, because a 24h-wide timeline is not legible at 10ft.
    static constexpr int kPageSeconds = 24 * 3600;
    static constexpr int kWindowSeconds = 4 * 3600;
    static constexpr int kTickSeconds = 30 * 60;
    static constexpr int kRows = 7;
    enum class View { Horizontal, Vertical, Live, Now, Next };

    std::vector<TimelineChannel> channels;
    time_t now = 0;
    time_t baseStart = 0;
    time_t visibleStart = 0;
    time_t focusTime = 0;
    int dayPage = 0;
    int channel = 0;
    int firstChannel = 0;
    int event = -1;
    View view = View::Horizontal;

    void Reset(time_t currentTime, int tunedChannel)
    {
        now = currentTime;
        baseStart = ((currentTime - 3600) / kTickSeconds) * kTickSeconds;
        visibleStart = ((currentTime - 1800) / kTickSeconds) * kTickSeconds;
        focusTime = currentTime;
        dayPage = 0;
        channel = 0;
        firstChannel = 0;
        view = View::Horizontal;
        for (size_t i = 0; i < channels.size(); ++i)
            if (channels[i].number == tunedChannel) {
                channel = int(i);
                break;
            }
        PickAt(focusTime);
        KeepVisible();
    }

    void KeepVisible()
    {
        if (channels.empty()) {
            channel = firstChannel = 0;
            event = -1;
            return;
        }
        channel = std::clamp(channel, 0, int(channels.size()) - 1);
        if (channel < firstChannel) firstChannel = channel;
        if (channel >= firstChannel + kRows)
            firstChannel = channel - kRows + 1;
        firstChannel = std::clamp(firstChannel, 0, int(channels.size()) - 1);
    }

    const TimelineEvent *Selected() const
    {
        if (channel < 0 || channel >= int(channels.size()))
            return nullptr;
        const auto &events = channels[channel].events;
        return event < 0 || event >= int(events.size()) ? nullptr : &events[event];
    }

    void PickAt(time_t at)
    {
        event = -1;
        if (channel < 0 || channel >= int(channels.size()))
            return;
        const auto &events = channels[channel].events;
        time_t closestDistance = kPageSeconds * 4;
        for (size_t i = 0; i < events.size(); ++i) {
            const auto &e = events[i];
            if (e.start <= at && e.end > at) {
                event = int(i);
                return;
            }
            const time_t distance = e.start > at ? e.start - at : at - e.end;
            if (distance < closestDistance) {
                closestDistance = distance;
                event = int(i);
            }
        }
    }

    void MoveChannel(int change)
    {
        if (channels.empty()) return;
        channel = std::clamp(channel + change, 0, int(channels.size()) - 1);
        PickAt(focusTime);
        KeepVisible();
    }

    void MoveEvent(int change)
    {
        if (channel < 0 || channel >= int(channels.size())) return;
        const int count = int(channels[channel].events.size());
        if (count <= 0) {
            ShiftWindow(change * kTickSeconds);
            return;
        }
        const int original = event;
        if (event < 0)
            event = change > 0 ? 0 : count - 1;
        else
            event = std::clamp(event + change, 0, count - 1);
        if (event == original && change != 0) {
            ShiftWindow(change * kTickSeconds);
            return;
        }
        const auto &selected = channels[channel].events[event];
        focusTime = selected.start + (selected.end - selected.start) / 2;
        if (selected.start < visibleStart)
            visibleStart = (selected.start / kTickSeconds) * kTickSeconds;
        if (selected.start >= visibleStart + kWindowSeconds)
            visibleStart = (selected.start / kTickSeconds) * kTickSeconds;
        dayPage = visibleStart >= baseStart + kPageSeconds ? 1 : 0;
    }

    void ShiftWindow(int seconds)
    {
        const time_t lower = baseStart;
        const time_t upper = baseStart + 2 * kPageSeconds - kWindowSeconds;
        visibleStart = std::clamp(visibleStart + seconds, lower, upper);
        dayPage = visibleStart >= baseStart + kPageSeconds ? 1 : 0;
        focusTime = visibleStart + kWindowSeconds / 2;
        PickAt(focusTime);
    }

    void SetPage(int page)
    {
        dayPage = std::clamp(page, 0, 1);
        visibleStart = baseStart + dayPage * kPageSeconds;
        focusTime = visibleStart + kWindowSeconds / 2;
        PickAt(focusTime);
    }

    void JumpNow()
    {
        dayPage = 0;
        visibleStart = ((now - 1800) / kTickSeconds) * kTickSeconds;
        focusTime = now;
        PickAt(focusTime);
    }

    void NextView()
    {
        const int next = (int(view) + 1) % 5;
        view = static_cast<View>(next);
    }

    // Geometry equivalent to epgEventPositionForBounds in web frontend:
    // intersect the event with the visible interval, then compute its width.
    bool Geometry(const TimelineEvent &e, double &left, double &span) const
    {
        const time_t start = std::max(e.start, visibleStart);
        const time_t end = std::min(e.end, visibleStart + kWindowSeconds);
        if (end <= start) return false;
        left = double(start - visibleStart) / kWindowSeconds;
        span = double(end - start) / kWindowSeconds;
        return true;
    }
};

} // namespace suiteosd

#endif // VDR_SUITEOSD_EPG_TIMELINE_H
