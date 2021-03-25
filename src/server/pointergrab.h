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
 * The PointerGrab class is the base class for pointer grabs.
 */
class KWAYLANDSERVER_EXPORT PointerGrab
{
public:
    explicit PointerGrab(SeatInterface *seat);
    virtual ~PointerGrab();

    bool isActive() const;
    void setActive(bool active);

    SeatInterface *seat() const;
    virtual void cancel();

    virtual void handleFocus(SurfaceInterface *surface, const QPointF &position, quint32 serial) = 0;
    virtual void handleButton(quint32 button, PointerButtonState state, quint32 serial) = 0;
    virtual void handleAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source) = 0;
    virtual void handleMotion(const QPointF &position) = 0;
    virtual void handleFrame() = 0;

private:
    QPointer<SeatInterface> m_seat;
};

} // namespace KWaylandServer
