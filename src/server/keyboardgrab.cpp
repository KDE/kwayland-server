/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keyboardgrab.h"
#include "seat_interface_p.h"

namespace KWaylandServer
{

KeyboardGrab::KeyboardGrab(SeatInterface *seat, QObject *parent)
    : QObject(parent)
    , m_seat(seat)
{
}

SeatInterface *KeyboardGrab::seat() const
{
    return m_seat;
}

void KeyboardGrab::activate()
{
}

void KeyboardGrab::deactivate()
{
}

bool KeyboardGrab::isActive() const
{
    return m_seat->keyboardGrab() == this;
}

void KeyboardGrab::setActive(bool active)
{
    if (isActive() == active) {
        return;
    }

    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(m_seat);
    if (active) {
        seatPrivate->grabKeyboard(this);
    } else {
        seatPrivate->ungrabKeyboard(this);
    }
}

} // namespace KWaylandServer
