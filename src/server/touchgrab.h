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
 * The TouchGrab class is the base class for touch device grabs.
 */
class KWAYLANDSERVER_EXPORT TouchGrab
{
public:
    explicit TouchGrab(SeatInterface *seat);
    virtual ~TouchGrab();

    bool isActive() const;
    void setActive(bool active);

    SeatInterface *seat() const;
    virtual void cancel();

    virtual void handleFocus(SurfaceInterface *surface) = 0;
    virtual void handleDown(qint32 id, quint32 serial, const QPointF &localPos) = 0;
    virtual void handleUp(qint32 id, quint32 serial) = 0;
    virtual void handleFrame() = 0;
    virtual void handleCancel() = 0;
    virtual void handleMotion(qint32 id, const QPointF &localPos) = 0;

private:
    QPointer<SeatInterface> m_seat;
};

} // namespace KWaylandServer
