/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointergestures_v1_interface.h"
#include "clientconnection.h"
#include "display.h"
#include "pointer_interface_p.h"
#include "pointergestures_v1_interface_p.h"
#include "seat_interface.h"
#include "surface_interface.h"

namespace KWaylandServer
{

static const int s_version = 1;

PointerGesturesV1InterfacePrivate::PointerGesturesV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_pointer_gestures_v1(*display, s_version)
{
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_swipe_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    PointerSwipeGestureV1Interface *swipeGesture = PointerSwipeGestureV1Interface::get(pointer);
    if (!swipeGesture) {
        swipeGesture = new PointerSwipeGestureV1Interface(pointer, pointer); // owned by wl_pointer
    }

    swipeGesture->add(resource->client(), id, resource->version());
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_pinch_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    PointerPinchGestureV1Interface *pinchGesture = PointerPinchGestureV1Interface::get(pointer);
    if (!pinchGesture) {
        pinchGesture = new PointerPinchGestureV1Interface(pointer, pointer); // owned by wl_pointer
    }

    pinchGesture->add(resource->client(), id, resource->version());
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PointerGesturesV1Interface::PointerGesturesV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PointerGesturesV1InterfacePrivate(display))
{
}

PointerGesturesV1Interface::~PointerGesturesV1Interface()
{
}

PointerSwipeGestureV1Interface::PointerSwipeGestureV1Interface(PointerInterface *pointer, QObject *parent)
    : QObject(parent)
    , pointer(pointer)
{
    PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
    pointerPrivate->swipeGesturesV1 = this;
}

PointerSwipeGestureV1Interface::~PointerSwipeGestureV1Interface()
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        pointerPrivate->swipeGesturesV1 = nullptr;
    }
}

PointerSwipeGestureV1Interface *PointerSwipeGestureV1Interface::get(PointerInterface *pointer)
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        return pointerPrivate->swipeGesturesV1;
    }
    return nullptr;
}

void PointerSwipeGestureV1Interface::zwp_pointer_gesture_swipe_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerSwipeGestureV1Interface::sendBegin(quint32 serial, quint32 fingerCount)
{
    if (focusedClient) {
        return;
    }
    if (!pointer->focusedSurface()) {
        return;
    }

    const SurfaceInterface *focusedSurface = pointer->focusedSurface();
    focusedClient = focusedSurface->client();
    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        if (swipeResource->client() == focusedClient->client()) {
            send_begin(swipeResource->handle, serial, seat->timestamp(), focusedSurface->resource(), fingerCount);
        }
    }
}

void PointerSwipeGestureV1Interface::sendUpdate(const QSizeF &delta)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        if (swipeResource->client() == focusedClient->client()) {
            send_update(swipeResource->handle, seat->timestamp(),
                        wl_fixed_from_double(delta.width()), wl_fixed_from_double(delta.height()));
        }
    }
}

void PointerSwipeGestureV1Interface::sendEnd(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        if (swipeResource->client() == focusedClient->client()) {
            send_end(swipeResource->handle, serial, seat->timestamp(), false);
        }
    }

    focusedClient = nullptr;
}

void PointerSwipeGestureV1Interface::sendCancel(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        if (swipeResource->client() == focusedClient->client()) {
            send_end(swipeResource->handle, serial, seat->timestamp(), true);
        }
    }

    focusedClient = nullptr;
}

PointerPinchGestureV1Interface::PointerPinchGestureV1Interface(PointerInterface *pointer, QObject *parent)
    : QObject(parent)
    , pointer(pointer)
{
    PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
    pointerPrivate->pinchGesturesV1 = this;
}

PointerPinchGestureV1Interface::~PointerPinchGestureV1Interface()
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        pointerPrivate->pinchGesturesV1 = nullptr;
    }
}

PointerPinchGestureV1Interface *PointerPinchGestureV1Interface::get(PointerInterface *pointer)
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        return pointerPrivate->pinchGesturesV1;
    }
    return nullptr;
}

void PointerPinchGestureV1Interface::zwp_pointer_gesture_pinch_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerPinchGestureV1Interface::sendBegin(quint32 serial, quint32 fingerCount)
{
    if (focusedClient) {
        return; // gesture is already active
    }
    if (!pointer->focusedSurface()) {
        return;
    }

    const SurfaceInterface *focusedSurface = pointer->focusedSurface();
    focusedClient = focusedSurface->client();
    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        if (pinchResource->client() == focusedClient->client()) {
            send_begin(pinchResource->handle, serial, seat->timestamp(), focusedSurface->resource(), fingerCount);
        }
    }
}

void PointerPinchGestureV1Interface::sendUpdate(const QSizeF &delta, qreal scale, qreal rotation)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        if (pinchResource->client() == focusedClient->client()) {
            send_update(pinchResource->handle, seat->timestamp(),
                        wl_fixed_from_double(delta.width()), wl_fixed_from_double(delta.height()),
                        wl_fixed_from_double(scale), wl_fixed_from_double(rotation));
        }
    }
}

void PointerPinchGestureV1Interface::sendEnd(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        if (pinchResource->client() == focusedClient->client()) {
            send_end(pinchResource->handle, serial, seat->timestamp(), false);
        }
    }

    focusedClient = nullptr;
}

void PointerPinchGestureV1Interface::sendCancel(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        if (pinchResource->client() == focusedClient->client()) {
            send_end(pinchResource->handle, serial, seat->timestamp(), true);
        }
    }

    focusedClient = nullptr;
}

} // namespace KWaylandServer
