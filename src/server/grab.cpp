#include "seat_interface.h"
#include "grab.h"
namespace KWaylandServer
{
Grab::Grab(QObject *parent) : QObject(parent) {}

GrabManager::GrabManager(SeatInterface *parent) : QObject(parent), m_seat(parent) {}

SurfaceInterface* GrabManager::computeCurrentGrab(GrabKinds forKind)
{
    for (auto [grab, _] : m_grabs) {
        const auto& [surf, kind] = m_currentRequests.value(grab);
        if (surf.has_value() && (int(kind) & int(forKind))) {
            return surf.value();
        }
    }

    return nullptr;
}

SurfaceInterface* GrabManager::currentGrab(GrabKinds kinds)
{
    return computeCurrentGrab(kinds);
}

void GrabManager::registerGrab(Grab* grab, int priority)
{
    grab->setSeat(m_seat);
    m_grabs << PriorityGrab(grab, priority);

    // Sort grabs so that we iterate over the ones with higher priority before the
    // ones with lower priority.
    std::sort(m_grabs.begin(), m_grabs.end(), [=](PriorityGrab& lhs, PriorityGrab& rhs) {
        auto [_, lhsPriority] = lhs;
        auto [__, rhsPriority] = rhs;

        return lhsPriority > rhsPriority;
    });

    connect(grab, &Grab::wantedGrabChanged, [=](OptionalSurface surf, GrabKinds kinds) {
        m_currentRequests[grab] = {surf, kinds};
        Q_EMIT currentGrabChanged(kinds);
    });
}

} // KWaylandServer