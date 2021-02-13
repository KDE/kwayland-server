/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>
#include <QPointF>

namespace KWaylandServer
{

class SeatInterface;
class SurfaceInterface;

class KWAYLANDSERVER_EXPORT TouchGrab : public QObject
{
    Q_OBJECT

public:
    explicit TouchGrab(SeatInterface *seat, QObject *parent = nullptr);

    bool isActive() const;
    void setActive(bool active);

    SeatInterface *seat() const;
    virtual void cancel() = 0;

    virtual void activate();
    virtual void deactivate();

    virtual void handleFocusChange(SurfaceInterface *surface) = 0;
    virtual void handleDown(qint32 id, quint32 serial, const QPointF &localPos) = 0;
    virtual void handleUp(qint32 id, quint32 serial) = 0;
    virtual void handleFrame() = 0;
    virtual void handleCancel() = 0;
    virtual void handleMotion(qint32 id, const QPointF &localPos) = 0;

private:
    SeatInterface *m_seat;
};

} // namespace KWaylandServer
