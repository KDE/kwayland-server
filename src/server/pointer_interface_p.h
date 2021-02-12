/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_POINTER_INTERFACE_P_H
#define WAYLAND_SERVER_POINTER_INTERFACE_P_H
#include "pointer_interface.h"

#include <QPointF>
#include <QPointer>
#include <QVector>

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{

class ClientConnection;
class PointerPinchGestureV1Interface;
class PointerSwipeGestureV1Interface;
class RelativePointerV1Interface;

class PointerInterfacePrivate : public QtWaylandServer::wl_pointer
{
public:
    static PointerInterfacePrivate *get(PointerInterface *pointer);
    PointerInterfacePrivate(PointerInterface *q, SeatInterface *seat);

    QList<Resource *> pointersForClient(ClientConnection *client) const;

    PointerInterface *q;
    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QMetaObject::Connection destroyConnection;
    Cursor *cursor = nullptr;
    RelativePointerV1Interface *relativePointersV1 = nullptr;
    PointerSwipeGestureV1Interface *swipeGesturesV1 = nullptr;
    PointerPinchGestureV1Interface *pinchGesturesV1 = nullptr;
    QPointF lastPosition;

    void sendLeave(quint32 serial);
    void sendEnter(const QPointF &parentSurfacePosition, quint32 serial);
    void sendFrame();

protected:
    void pointer_set_cursor(Resource *resource, uint32_t serial, ::wl_resource *surface_resource,
                            int32_t hotspot_x, int32_t hotspot_y) override;
    void pointer_release(Resource *resource) override;
    void pointer_bind_resource(Resource *resource) override;
};

}

#endif
