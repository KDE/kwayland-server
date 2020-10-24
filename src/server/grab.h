/*
    SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_GRAB_H
#define KWAYLAND_SERVER_GRAB_H

#include <QObject>
#include <optional>

#include <KWaylandServer/kwaylandserver_export.h>
#include "surface_interface.h"

namespace KWaylandServer
{

class SeatInterface;

using OptionalSurface = std::optional<SurfaceInterface*>;

enum class GrabKinds
{
    None     = 0b00000000000000000000000000000000,
    Keyboard = 0b00000000000000000000000000000001,
    Pointer  = 0b00000000000000000000000000000010,
    Touch    = 0b00000000000000000000000000000100,
};

inline constexpr GrabKinds
operator&(GrabKinds lhs, GrabKinds rhs)
{
    return GrabKinds(int(lhs) & int(rhs));
}

inline constexpr GrabKinds
operator|(GrabKinds lhs, GrabKinds rhs)
{
    return GrabKinds(int(lhs) | int(rhs));
}

using KindedOptionalSurface = std::tuple<OptionalSurface, GrabKinds>;

class KWAYLANDSERVER_EXPORT Grab : public QObject
{
    Q_OBJECT

private:
    SeatInterface* m_seat = nullptr;

    void setSeat(SeatInterface* seat) { m_seat = seat; }
    friend class GrabManager;

public:
    SeatInterface* seat() const { return m_seat; }
    Grab(QObject *parent);

    Q_SIGNAL void wantedGrabChanged(OptionalSurface surf, GrabKinds kinds);
};

class KWAYLANDSERVER_EXPORT GrabManager : public QObject
{
    Q_OBJECT

private:
    SeatInterface* m_seat;
    SurfaceInterface* m_currentGrab;

    using PriorityGrab = std::tuple<Grab*,int>;

    QList<PriorityGrab> m_grabs;
    QHash<Grab*,KindedOptionalSurface> m_currentRequests;
    SurfaceInterface* computeCurrentGrab(GrabKinds forKind);

public:
    GrabManager(SeatInterface *parent);

    SurfaceInterface* currentGrab(GrabKinds kinds);
    Q_SIGNAL void currentGrabChanged(GrabKinds kinds);

    void registerGrab(Grab* grab, int priority);

    template<typename Kind, int Priority>
    Kind* handler() {
        static_assert(std::is_convertible<Kind*,Grab*>::value, "Kind must inherit KWaylandServer::Grab");

        for (auto& [grab, _] : m_grabs) {
            if (auto g = dynamic_cast<Kind*>(grab)) {
                return g;
            }
        }

        auto ret = new Kind(this);
        registerGrab(ret, Priority);

        return ret;
    }
};

} // KWaylandServer

#endif