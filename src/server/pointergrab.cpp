/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointergrab.h"
#include "seat_interface_p.h"

namespace KWaylandServer
{

PointerGrab::PointerGrab(SeatInterface *seat)
    : m_seat(seat)
{
}

PointerGrab::~PointerGrab()
{
    setActive(false);
}

SeatInterface *PointerGrab::seat() const
{
    return m_seat;
}

bool PointerGrab::isActive() const
{
    if (m_seat) {
        return m_seat->pointerGrab() == this;
    }
    return false;
}

void PointerGrab::setActive(bool active)
{
    if (!m_seat) {
        return;
    }

    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(m_seat);
    if (active) {
        seatPrivate->grabPointer(this);
    } else {
        seatPrivate->ungrabPointer(this);
    }
}

void PointerGrab::cancel()
{
}

} // namespace KWaylandServer
