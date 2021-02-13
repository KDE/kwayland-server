/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointergrab.h"
#include "seat_interface_p.h"

namespace KWaylandServer
{

PointerGrab::PointerGrab(SeatInterface *seat, QObject *parent)
    : QObject(parent)
    , m_seat(seat)
{
}

SeatInterface *PointerGrab::seat() const
{
    return m_seat;
}

void PointerGrab::activate()
{
}

void PointerGrab::deactivate()
{
}

bool PointerGrab::isActive() const
{
    return m_seat->pointerGrab() == this;
}

void PointerGrab::setActive(bool active)
{
    if (isActive() == active) {
        return;
    }

    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(m_seat);
    if (active) {
        seatPrivate->grabPointer(this);
    } else {
        seatPrivate->ungrabPointer(this);
    }
}

} // namespace KWaylandServer
