/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

namespace KWaylandServer
{

class SeatInterface;
class SurfaceInterface;
class TouchInterfacePrivate;

/**
 * The TouchInterface class repserents a touchscreen associated with a wl_seat.
 *
 * The TouchInterface corresponds to the Wayland interface @c wl_touch.
 */
class KWAYLANDSERVER_EXPORT TouchInterface : public QObject
{
    Q_OBJECT

public:
    explicit TouchInterface(SeatInterface *seat);
    ~TouchInterface() override;

    SurfaceInterface *focusedSurface() const;
    void setFocusedSurface(SurfaceInterface *surface);

    void sendDown(qint32 id, quint32 serial, const QPointF &localPos);
    void sendUp(qint32 id, quint32 serial);
    void sendFrame();
    void sendCancel();
    void sendMotion(qint32 id, const QPointF &localPos);

private:
    QScopedPointer<TouchInterfacePrivate> d;
    friend class TouchInterfacePrivate;
};

} // namespace KWaylandServer

Q_DECLARE_METATYPE(KWaylandServer::TouchInterface*)
