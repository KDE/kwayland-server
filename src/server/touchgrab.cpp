/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "touchgrab.h"
#include "seat_interface_p.h"

namespace KWaylandServer
{

TouchGrab::TouchGrab(SeatInterface *seat, QObject *parent)
    : QObject(parent)
    , m_seat(seat)
{
}

SeatInterface *TouchGrab::seat() const
{
    return m_seat;
}

void TouchGrab::activate()
{
}

void TouchGrab::deactivate()
{
}

bool TouchGrab::isActive() const
{
    return m_seat->touchGrab() == this;
}

void TouchGrab::setActive(bool active)
{
    if (isActive() == active) {
        return;
    }

    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(m_seat);
    if (active) {
        seatPrivate->grabTouch(this);
    } else {
        seatPrivate->ungrabTouch(this);
    }
}

} // namespace KWaylandServer
