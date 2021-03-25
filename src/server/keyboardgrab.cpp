/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keyboardgrab.h"
#include "seat_interface_p.h"

namespace KWaylandServer
{

KeyboardGrab::KeyboardGrab(SeatInterface *seat)
    : m_seat(seat)
{
}

KeyboardGrab::~KeyboardGrab()
{
    setActive(false);
}

SeatInterface *KeyboardGrab::seat() const
{
    return m_seat;
}

bool KeyboardGrab::isActive() const
{
    if (m_seat) {
        return m_seat->keyboardGrab() == this;
    }
    return false;
}

void KeyboardGrab::setActive(bool active)
{
    if (!m_seat) {
        return;
    }

    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(m_seat);
    if (active) {
        seatPrivate->grabKeyboard(this);
    } else {
        seatPrivate->ungrabKeyboard(this);
    }
}

void KeyboardGrab::cancel()
{
}

} // namespace KWaylandServer
