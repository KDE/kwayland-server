/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "seat_interface.h"

#include <QPointer>

namespace KWaylandServer
{

/**
 * The KeyboardGrab class is the base class for keyboard grabs.
 */
class KWAYLANDSERVER_EXPORT KeyboardGrab
{
public:
    explicit KeyboardGrab(SeatInterface *seat);
    virtual ~KeyboardGrab();

    bool isActive() const;
    void setActive(bool active);

    SeatInterface *seat() const;
    virtual void cancel();

    virtual void handleFocus(SurfaceInterface *surface, quint32 serial) = 0;
    virtual void handleKey(quint32 keyCode, KeyboardKeyState state) = 0;
    virtual void handleModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group) = 0;

private:
    QPointer<SeatInterface> m_seat;
};

} // namespace KWaylandServer
