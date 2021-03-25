/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "touchgrab.h"
#include "seat_interface_p.h"

namespace KWaylandServer
{

TouchGrab::TouchGrab(SeatInterface *seat)
    : m_seat(seat)
{
}

TouchGrab::~TouchGrab()
{
    setActive(false);
}

SeatInterface *TouchGrab::seat() const
{
    return m_seat;
}

bool TouchGrab::isActive() const
{
    if (m_seat) {
        return m_seat->touchGrab() == this;
    }
    return false;
}

void TouchGrab::setActive(bool active)
{
    if (!m_seat) {
        return;
    }

    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(m_seat);
    if (active) {
        seatPrivate->grabTouch(this);
    } else {
        seatPrivate->ungrabTouch(this);
    }
}

void TouchGrab::cancel()
{
}

} // namespace KWaylandServer
