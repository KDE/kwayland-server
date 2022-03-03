/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointer_interface.h"
#include "clientconnection.h"
#include "display.h"
#include "logging.h"
#include "pointer_interface_p.h"
#include "pointergestures_v1_interface_p.h"
#include "relativepointer_v1_interface_p.h"
#include "seat_interface.h"
#include "surface_interface.h"
#include "surfacerole_p.h"
#include "utils.h"

namespace KWaylandServer
{
class CursorPrivate
{
public:
    CursorPrivate(Cursor *q, PointerInterface *pointer);

    Cursor *q;
    PointerInterface *pointer;
    quint32 enteredSerial = 0;
    QPoint hotspot;
    QPointer<SurfaceInterface> surface;

    void update(SurfaceInterface *surface, quint32 serial, const QPoint &hotspot);
};

PointerInterfacePrivate *PointerInterfacePrivate::get(PointerInterface *pointer)
{
    return pointer->d.data();
}

PointerInterfacePrivate::PointerInterfacePrivate(PointerInterface *q, SeatInterface *seat)
    : q(q)
    , seat(seat)
    , relativePointersV1(new RelativePointerV1Interface(q))
    , swipeGesturesV1(new PointerSwipeGestureV1Interface(q))
    , pinchGesturesV1(new PointerPinchGestureV1Interface(q))
    , holdGesturesV1(new PointerHoldGestureV1Interface(q))
{
}

PointerInterfacePrivate::~PointerInterfacePrivate()
{
}

QList<PointerInterfacePrivate::Resource *> PointerInterfacePrivate::pointersForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

void PointerInterfacePrivate::pointer_set_cursor(Resource *resource, uint32_t serial, ::wl_resource *surface_resource, int32_t hotspot_x, int32_t hotspot_y)
{
    SurfaceInterface *surface = nullptr;

    if (!focusedSurface) {
        return;
    }
    if (focusedSurface->client()->client() != resource->client()) {
        qCDebug(KWAYLAND_SERVER, "Denied set_cursor request from unfocused client");
        return;
    }

    if (surface_resource) {
        surface = SurfaceInterface::get(surface_resource);
        if (!surface) {
            wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid surface");
            return;
        }

        const SurfaceRole *surfaceRole = SurfaceRole::get(surface);
        if (surfaceRole) {
            wl_resource_post_error(resource->handle, error_role, "the wl_surface already has a role assigned %s", surfaceRole->name().constData());
            return;
        }
    }

    if (!cursor) { // TODO: Assign the cursor surface role.
        cursor = new Cursor(q);
        cursor->d->update(surface, serial, QPoint(hotspot_x, hotspot_y));
        QObject::connect(cursor, &Cursor::changed, q, &PointerInterface::cursorChanged);
        Q_EMIT q->cursorChanged();
    } else {
        cursor->d->update(surface, serial, QPoint(hotspot_x, hotspot_y));
    }
}

void PointerInterfacePrivate::pointer_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerInterfacePrivate::pointer_bind_resource(Resource *resource)
{
    const ClientConnection *focusedClient = focusedSurface ? focusedSurface->client() : nullptr;

    if (focusedClient && focusedClient->client() == resource->client()) {
        const quint32 serial = seat->display()->nextSerial();
        send_enter(resource->handle, serial, focusedSurface->resource(), wl_fixed_from_double(lastPosition.x()), wl_fixed_from_double(lastPosition.y()));
        if (resource->version() >= WL_POINTER_FRAME_SINCE_VERSION) {
            send_frame(resource->handle);
        }
    }
}

void PointerInterfacePrivate::sendLeave(quint32 serial)
{
    const QList<Resource *> pointerResources = pointersForClient(focusedSurface->client());
    for (Resource *resource : pointerResources) {
        send_leave(resource->handle, serial, focusedSurface->resource());
    }
}

void PointerInterfacePrivate::sendEnter(const QPointF &position, quint32 serial)
{
    const QList<Resource *> pointerResources = pointersForClient(focusedSurface->client());
    for (Resource *resource : pointerResources) {
        send_enter(resource->handle, serial, focusedSurface->resource(), wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
    }
}

void PointerInterfacePrivate::sendFrame()
{
    const QList<Resource *> pointerResources = pointersForClient(focusedSurface->client());
    for (Resource *resource : pointerResources) {
        if (resource->version() >= WL_POINTER_FRAME_SINCE_VERSION) {
            send_frame(resource->handle);
        }
    }
}

PointerInterface::PointerInterface(SeatInterface *seat)
    : d(new PointerInterfacePrivate(this, seat))
{
}

PointerInterface::~PointerInterface()
{
}

SurfaceInterface *PointerInterface::focusedSurface() const
{
    return d->focusedSurface;
}

void PointerInterface::sendEnter(SurfaceInterface *surface, const QPointF &position, quint32 serial)
{
    if (d->focusedSurface == surface) {
        return;
    }

    if (d->focusedSurface) {
        d->sendLeave(serial);
        if (d->focusedSurface->client() != surface->client()) {
            d->sendFrame();
        }
        disconnect(d->destroyConnection);
    }

    d->focusedSurface = surface;
    d->destroyConnection = connect(d->focusedSurface, &SurfaceInterface::aboutToBeDestroyed, this, [this]() {
        d->sendLeave(d->seat->display()->nextSerial());
        d->sendFrame();
        d->focusedSurface = nullptr;
        Q_EMIT focusedSurfaceChanged();
    });

    d->sendEnter(position, serial);
    d->sendFrame();
    d->lastPosition = position;

    Q_EMIT focusedSurfaceChanged();
}

void PointerInterface::sendLeave(quint32 serial)
{
    if (!d->focusedSurface) {
        return;
    }

    d->sendLeave(serial);
    d->sendFrame();

    d->focusedSurface = nullptr;
    disconnect(d->destroyConnection);

    Q_EMIT focusedSurfaceChanged();
}

void PointerInterface::sendButton(quint32 button, PointerButtonState state, quint32 serial)
{
    if (!d->focusedSurface) {
        return;
    }

    const auto pointerResources = d->pointersForClient(d->focusedSurface->client());
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        d->send_button(resource->handle, serial, d->seat->timestamp(), button, quint32(state));
    }
}

void PointerInterface::sendAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source)
{
    if (!d->focusedSurface) {
        return;
    }

    const auto pointerResources = d->pointersForClient(d->focusedSurface->client());
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        const quint32 version = resource->version();

        const auto wlOrientation =
            (orientation == Qt::Vertical) ? PointerInterfacePrivate::axis_vertical_scroll : PointerInterfacePrivate::axis_horizontal_scroll;

        if (source != PointerAxisSource::Unknown && version >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
            PointerInterfacePrivate::axis_source wlSource;
            switch (source) {
            case PointerAxisSource::Wheel:
                wlSource = PointerInterfacePrivate::axis_source_wheel;
                break;
            case PointerAxisSource::Finger:
                wlSource = PointerInterfacePrivate::axis_source_finger;
                break;
            case PointerAxisSource::Continuous:
                wlSource = PointerInterfacePrivate::axis_source_continuous;
                break;
            case PointerAxisSource::WheelTilt:
                wlSource = PointerInterfacePrivate::axis_source_wheel_tilt;
                break;
            default:
                Q_UNREACHABLE();
                break;
            }
            d->send_axis_source(resource->handle, wlSource);
        }

        if (delta != 0.0) {
            if (discreteDelta && version >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION) {
                d->send_axis_discrete(resource->handle, wlOrientation, discreteDelta);
            }
            d->send_axis(resource->handle, d->seat->timestamp(), wlOrientation, wl_fixed_from_double(delta));
        } else if (version >= WL_POINTER_AXIS_STOP_SINCE_VERSION) {
            d->send_axis_stop(resource->handle, d->seat->timestamp(), wlOrientation);
        }
    }
}

void PointerInterface::sendMotion(const QPointF &position)
{
    d->lastPosition = position;

    if (!d->focusedSurface) {
        return;
    }

    const auto pointerResources = d->pointersForClient(d->focusedSurface->client());
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        d->send_motion(resource->handle, d->seat->timestamp(), wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
    }
}

void PointerInterface::sendFrame()
{
    if (d->focusedSurface) {
        d->sendFrame();
    }
}

Cursor *PointerInterface::cursor() const
{
    return d->cursor;
}

SeatInterface *PointerInterface::seat() const
{
    return d->seat;
}

PointerInterface *PointerInterface::get(wl_resource *native)
{
    if (PointerInterfacePrivate *pointerPrivate = resource_cast<PointerInterfacePrivate *>(native)) {
        return pointerPrivate->q;
    }
    return nullptr;
}

CursorPrivate::CursorPrivate(Cursor *q, PointerInterface *pointer)
    : q(q)
    , pointer(pointer)
{
}

void CursorPrivate::update(SurfaceInterface *s, quint32 serial, const QPoint &p)
{
    bool emitChanged = false;
    if (enteredSerial != serial) {
        enteredSerial = serial;
        emitChanged = true;
        Q_EMIT q->enteredSerialChanged();
    }
    if (hotspot != p) {
        hotspot = p;
        emitChanged = true;
        Q_EMIT q->hotspotChanged();
    }
    if (surface != s) {
        if (!surface.isNull()) {
            QObject::disconnect(surface.data(), &SurfaceInterface::damaged, q, &Cursor::changed);
        }
        surface = s;
        if (!surface.isNull()) {
            QObject::connect(surface.data(), &SurfaceInterface::damaged, q, &Cursor::changed);
        }
        emitChanged = true;
        Q_EMIT q->surfaceChanged();
    }
    if (emitChanged) {
        Q_EMIT q->changed();
    }
}

Cursor::Cursor(PointerInterface *parent)
    : QObject(parent)
    , d(new CursorPrivate(this, parent))
{
}

Cursor::~Cursor()
{
}

quint32 Cursor::enteredSerial() const
{
    return d->enteredSerial;
}

QPoint Cursor::hotspot() const
{
    return d->hotspot;
}

PointerInterface *Cursor::pointer() const
{
    return d->pointer;
}

SurfaceInterface *Cursor::surface() const
{
    return d->surface;
}

} // namespace KWaylandServer
