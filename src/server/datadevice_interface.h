/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

#include "abstract_drop_handler.h"

struct wl_client;
struct wl_resource;

namespace KWaylandServer
{
class DataDeviceManagerInterface;
class DataOfferInterface;
class DataSourceInterface;
class AbstractDataSource;
class SeatInterface;
class SurfaceInterface;
class DataDeviceInterfacePrivate;
class DragAndDropIconPrivate;

/**
 * The DragAndDropIcon class represents a drag-and-drop icon.
 *
 * Note that the lifetime of the drag-and-drop icon is bound to the lifetime of the underlying
 * icon surface.
 */
class KWAYLANDSERVER_EXPORT DragAndDropIcon : public QObject
{
    Q_OBJECT

public:
    ~DragAndDropIcon() override;

    /**
     * Returns the position of the icon relative to the cursor's hotspot.
     */
    QPoint position() const;

    /**
     * Returns the underlying icon surface. This function always returns a valid surface.
     */
    SurfaceInterface *surface() const;

private:
    explicit DragAndDropIcon(SurfaceInterface *surface);
    friend class DataDeviceInterfacePrivate;
    QScopedPointer<DragAndDropIconPrivate> d;
};

/**
 * @brief DataDeviceInterface allows clients to share data by copy-and-paste and drag-and-drop.
 *
 * The data device is per seat.
 * Copy-and-paste use the selection functions.
 *
 * Represents the Resource for the wl_data_device interface.
 *
 * @see SeatInterface
 * @see DataSourceInterface
 */
class KWAYLANDSERVER_EXPORT DataDeviceInterface : public AbstractDropHandler
{
    Q_OBJECT
public:
    virtual ~DataDeviceInterface();

    SeatInterface *seat() const;

    DataSourceInterface *selection() const;

    void sendSelection(KWaylandServer::AbstractDataSource *other);
    void sendClearSelection();
    /**
     * The event is sent when a drag-and-drop operation is ended because the implicit grab is removed.
     */
    void drop() override;
    /**
     * Updates the SurfaceInterface to which drag motion events are sent.
     *
     * If a SurfaceInterface was registered in this DataDeviceInterface for drag motion events, it
     * will be sent a leave event.
     *
     * If @p surface is not null it will be sent a drag enter event.
     *
     * @param surface The SurfaceInterface which gets motion events
     * @param serial The serial to be used for enter/leave
     */
    void updateDragTarget(SurfaceInterface *surface, quint32 serial) override;
    void updateProxy(SurfaceInterface *remote);

    wl_client *client();

Q_SIGNALS:
    void aboutToBeDestroyed();
    void dragStarted(KWaylandServer::AbstractDataSource *source,
                     KWaylandServer::SurfaceInterface *originSurface,
                     quint32 serial,
                     KWaylandServer::DragAndDropIcon *dragIcon);
    void selectionChanged(KWaylandServer::DataSourceInterface *);
    void selectionCleared();

private:
    friend class DataDeviceManagerInterfacePrivate;
    explicit DataDeviceInterface(SeatInterface *seat, wl_resource *resource);
    QScopedPointer<DataDeviceInterfacePrivate> d;
    friend class DataDeviceInterfacePrivate;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataDeviceInterface *)
