/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-pointer-gestures-unstable-v1.h"

#include <QPointer>

namespace KWaylandServer
{

class ClientConnection;
class Display;
class PointerInterface;
class SurfaceInterface;

class PointerGesturesV1InterfacePrivate : public QtWaylandServer::zwp_pointer_gestures_v1
{
public:
    explicit PointerGesturesV1InterfacePrivate(Display *display);

protected:
    void zwp_pointer_gestures_v1_get_swipe_gesture(Resource *resource, uint32_t id,
                                                   struct ::wl_resource *pointer_resource) override;
    void zwp_pointer_gestures_v1_get_pinch_gesture(Resource *resource, uint32_t id,
                                                   struct ::wl_resource *pointer_resource) override;
    void zwp_pointer_gestures_v1_release(Resource *resource) override;
};

class PointerSwipeGestureV1Interface : public QObject, public QtWaylandServer::zwp_pointer_gesture_swipe_v1
{
    Q_OBJECT

public:
    explicit PointerSwipeGestureV1Interface(PointerInterface *pointer, QObject *parent = nullptr);
    ~PointerSwipeGestureV1Interface() override;

    static PointerSwipeGestureV1Interface *get(PointerInterface *pointer);

    void sendBegin(quint32 serial, quint32 fingerCount);
    void sendUpdate(const QSizeF &delta);
    void sendEnd(quint32 serial);
    void sendCancel(quint32 serial);

protected:
    void zwp_pointer_gesture_swipe_v1_destroy(Resource *resource) override;

private:
    QPointer<PointerInterface> pointer;
    QPointer<ClientConnection> focusedClient;
};

class PointerPinchGestureV1Interface : public QObject, public QtWaylandServer::zwp_pointer_gesture_pinch_v1
{
    Q_OBJECT

public:
    explicit PointerPinchGestureV1Interface(PointerInterface *pointer, QObject *parent = nullptr);
    ~PointerPinchGestureV1Interface() override;

    static PointerPinchGestureV1Interface *get(PointerInterface *pointer);

    void sendBegin(quint32 serial, quint32 fingerCount);
    void sendUpdate(const QSizeF &delta, qreal scale, qreal rotation);
    void sendEnd(quint32 serial);
    void sendCancel(quint32 serial);

protected:
    void zwp_pointer_gesture_pinch_v1_destroy(Resource *resource) override;

private:
    QPointer<PointerInterface> pointer;
    QPointer<ClientConnection> focusedClient;
};

} // namespace KWaylandServer
