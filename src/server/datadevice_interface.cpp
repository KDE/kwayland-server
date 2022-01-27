/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datadevice_interface.h"
#include "datadevice_interface_p.h"
#include "datadevicemanager_interface.h"
#include "dataoffer_interface.h"
#include "datasource_interface.h"
#include "display.h"
#include "pointer_interface.h"
#include "seat_interface.h"
#include "seat_interface_p.h"
#include "surface_interface.h"
#include "surfacerole_p.h"

namespace KWaylandServer
{
class DragAndDropIconPrivate : public SurfaceRole
{
public:
    explicit DragAndDropIconPrivate(SurfaceInterface *surface);

    void commit() override;

    QPoint position;
};

DragAndDropIconPrivate::DragAndDropIconPrivate(SurfaceInterface *surface)
    : SurfaceRole(surface, QByteArrayLiteral("dnd_icon"))
{
}

void DragAndDropIconPrivate::commit()
{
    position += surface()->offset();
}

DragAndDropIcon::DragAndDropIcon(SurfaceInterface *surface)
    : QObject(surface)
    , d(new DragAndDropIconPrivate(surface))
{
}

DragAndDropIcon::~DragAndDropIcon()
{
}

QPoint DragAndDropIcon::position() const
{
    return d->position;
}

SurfaceInterface *DragAndDropIcon::surface() const
{
    return d->surface();
}

DataDeviceInterfacePrivate *DataDeviceInterfacePrivate::get(DataDeviceInterface *device)
{
    return device->d.data();
}

DataDeviceInterfacePrivate::DataDeviceInterfacePrivate(SeatInterface *seat, DataDeviceInterface *_q, wl_resource *resource)
    : QtWaylandServer::wl_data_device(resource)
    , seat(seat)
    , q(_q)
{
}

void DataDeviceInterfacePrivate::data_device_start_drag(Resource *resource,
                                                        wl_resource *sourceResource,
                                                        wl_resource *originResource,
                                                        wl_resource *iconResource,
                                                        uint32_t serial)
{
    SurfaceInterface *iconSurface = SurfaceInterface::get(iconResource);

    const SurfaceRole *surfaceRole = SurfaceRole::get(iconSurface);
    if (surfaceRole) {
        wl_resource_post_error(resource->handle, error_role, "the icon surface already has a role assigned %s", surfaceRole->name().constData());
        return;
    }

    SurfaceInterface *focusSurface = SurfaceInterface::get(originResource);
    DataSourceInterface *dataSource = nullptr;
    if (sourceResource) {
        dataSource = DataSourceInterface::get(sourceResource);
    }

    const bool pointerGrab = seat->hasImplicitPointerGrab(serial) && seat->focusedPointerSurface() == focusSurface;
    if (!pointerGrab) {
        // Client doesn't have pointer grab.
        const bool touchGrab = seat->hasImplicitTouchGrab(serial) && seat->focusedTouchSurface() == focusSurface;
        if (!touchGrab) {
            // Client neither has pointer nor touch grab. No drag start allowed.
            return;
        }
    }

    DragAndDropIcon *dragIcon = nullptr;
    if (iconSurface) {
        // drag icon lifespan is mapped to surface lifespan
        dragIcon = new DragAndDropIcon(iconSurface);
    }
    drag.serial = serial;
    Q_EMIT q->dragStarted(dataSource, focusSurface, serial, dragIcon);
}

void DataDeviceInterfacePrivate::data_device_set_selection(Resource *resource, wl_resource *source, uint32_t serial)
{
    Q_UNUSED(resource)
    Q_UNUSED(serial)
    DataSourceInterface *dataSource = DataSourceInterface::get(source);

    if (dataSource && dataSource->supportedDragAndDropActions() && wl_resource_get_version(dataSource->resource()) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        wl_resource_post_error(dataSource->resource(), QtWaylandServer::wl_data_source::error_invalid_source, "Data source is for drag and drop");
        return;
    }

    if (selection == dataSource) {
        return;
    }
    if (selection) {
        selection->cancel();
    }
    selection = dataSource;
    if (selection) {
        Q_EMIT q->selectionChanged(selection);
    } else {
        Q_EMIT q->selectionCleared();
    }
}

void DataDeviceInterfacePrivate::data_device_release(QtWaylandServer::wl_data_device::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataOfferInterface *DataDeviceInterfacePrivate::createDataOffer(AbstractDataSource *source)
{
    if (!source) {
        // a data offer can only exist together with a source
        return nullptr;
    }

    wl_resource *data_offer_resource = wl_resource_create(resource()->client(), &wl_data_offer_interface, resource()->version(), 0);
    if (!data_offer_resource) {
        wl_resource_post_no_memory(resource()->handle);
        return nullptr;
    }

    DataOfferInterface *offer = new DataOfferInterface(source, data_offer_resource);
    send_data_offer(offer->resource());
    offer->sendAllOffers();
    return offer;
}

void DataDeviceInterfacePrivate::data_device_destroy_resource(QtWaylandServer::wl_data_device::Resource *resource)
{
    Q_UNUSED(resource)
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

DataDeviceInterface::DataDeviceInterface(SeatInterface *seat, wl_resource *resource)
    : AbstractDropHandler(nullptr)
    , d(new DataDeviceInterfacePrivate(seat, this, resource))
{
    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(seat);
    seatPrivate->registerDataDevice(this);
}

DataDeviceInterface::~DataDeviceInterface() = default;

SeatInterface *DataDeviceInterface::seat() const
{
    return d->seat;
}

DataSourceInterface *DataDeviceInterface::selection() const
{
    return d->selection;
}

void DataDeviceInterface::sendSelection(AbstractDataSource *other)
{
    auto r = d->createDataOffer(other);
    if (!r) {
        return;
    }
    d->send_selection(r->resource());
}

void DataDeviceInterface::sendClearSelection()
{
    d->send_selection(nullptr);
}

void DataDeviceInterface::drop()
{
    d->send_drop();
    if (d->drag.posConnection) {
        disconnect(d->drag.posConnection);
        d->drag.posConnection = QMetaObject::Connection();
    }
    disconnect(d->drag.destroyConnection);
    d->drag.destroyConnection = QMetaObject::Connection();
    d->drag.surface = nullptr;
}

void DataDeviceInterface::updateDragTarget(SurfaceInterface *surface, quint32 serial)
{
    if (d->drag.surface) {
        if (d->drag.surface->resource()) {
            d->send_leave();
        }
        if (d->drag.posConnection) {
            disconnect(d->drag.posConnection);
            d->drag.posConnection = QMetaObject::Connection();
        }
        disconnect(d->drag.destroyConnection);
        d->drag.destroyConnection = QMetaObject::Connection();
        d->drag.surface = nullptr;
        if (d->drag.sourceActionConnection) {
            disconnect(d->drag.sourceActionConnection);
            d->drag.sourceActionConnection = QMetaObject::Connection();
        }
        if (d->drag.targetActionConnection) {
            disconnect(d->drag.targetActionConnection);
            d->drag.targetActionConnection = QMetaObject::Connection();
        }
        // don't update serial, we need it
    }
    auto dragSource = d->seat->dragSource();
    if (!surface || !dragSource) {
        if (auto s = dragSource) {
            s->dndAction(DataDeviceManagerInterface::DnDAction::None);
        }
        return;
    }

    if (dragSource) {
        dragSource->accept(QString());
    }
    DataOfferInterface *offer = d->createDataOffer(dragSource);
    d->drag.surface = surface;
    if (d->seat->isDragPointer()) {
        d->drag.posConnection = connect(d->seat, &SeatInterface::pointerPosChanged, this, [this] {
            const QPointF pos = d->seat->dragSurfaceTransformation().map(d->seat->pointerPos());
            d->send_motion(d->seat->timestamp(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
        });
    } else if (d->seat->isDragTouch()) {
        // When dragging from one window to another, we may end up in a data_device
        // that didn't get "data_device_start_drag". In that case, the internal
        // touch point serial will be incorrect and we need to update it to the
        // serial from the seat.
        SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(seat());
        if (seatPrivate->drag.dragImplicitGrabSerial != d->drag.serial) {
            d->drag.serial = seatPrivate->drag.dragImplicitGrabSerial.value();
        }

        d->drag.posConnection = connect(d->seat, &SeatInterface::touchMoved, this, [this](qint32 id, quint32 serial, const QPointF &globalPosition) {
            Q_UNUSED(id);
            if (serial != d->drag.serial) {
                // different touch down has been moved
                return;
            }
            const QPointF pos = d->seat->dragSurfaceTransformation().map(globalPosition);
            d->send_motion(d->seat->timestamp(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
        });
    }
    d->drag.destroyConnection = connect(d->drag.surface, &QObject::destroyed, this, [this] {
        d->send_leave();
        if (d->drag.posConnection) {
            disconnect(d->drag.posConnection);
        }
        d->drag = DataDeviceInterfacePrivate::Drag();
    });

    QPointF pos;
    if (d->seat->isDragPointer()) {
        pos = d->seat->dragSurfaceTransformation().map(d->seat->pointerPos());
    } else if (d->seat->isDragTouch()) {
        pos = d->seat->dragSurfaceTransformation().map(d->seat->firstTouchPointPosition());
    }
    d->send_enter(serial, surface->resource(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()), offer ? offer->resource() : nullptr);
    if (offer) {
        offer->sendSourceActions();
        auto matchOffers = [dragSource, offer] {
            DataDeviceManagerInterface::DnDAction action{DataDeviceManagerInterface::DnDAction::None};
            if (dragSource->supportedDragAndDropActions().testFlag(offer->preferredDragAndDropAction())) {
                action = offer->preferredDragAndDropAction();
            } else {
                if (dragSource->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Copy)
                    && offer->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Copy)) {
                    action = DataDeviceManagerInterface::DnDAction::Copy;
                } else if (dragSource->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Move)
                           && offer->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Move)) {
                    action = DataDeviceManagerInterface::DnDAction::Move;
                } else if (dragSource->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Ask)
                           && offer->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Ask)) {
                    action = DataDeviceManagerInterface::DnDAction::Ask;
                }
            }
            offer->dndAction(action);
            dragSource->dndAction(action);
        };
        d->drag.targetActionConnection = connect(offer, &DataOfferInterface::dragAndDropActionsChanged, dragSource, matchOffers);
        d->drag.sourceActionConnection = connect(dragSource, &AbstractDataSource::supportedDragAndDropActionsChanged, dragSource, matchOffers);
    }
}

void DataDeviceInterface::updateProxy(SurfaceInterface *remote)
{
    // TODO: connect destroy signal?
    d->proxyRemoteSurface = remote;
}

wl_client *DataDeviceInterface::client()
{
    return d->resource()->client();
}

}
