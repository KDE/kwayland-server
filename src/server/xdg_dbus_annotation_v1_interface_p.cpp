/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdg_dbus_annotation_v1_interface_p.h"
#include "resource.h"
#include "xdgshell_interface.h"

namespace KWaylandServer
{

XdgDBusAnnotationManagerInterface::Private::Private(XdgDBusAnnotationManagerInterface *pub, Display *display)
    : QtWaylandServer::xdg_dbus_annotation_manager_v1(*display, s_version)
    , q(pub)
{

}

void XdgDBusAnnotationManagerInterface::Private::xdg_dbus_annotation_manager_v1_create(Resource *resource, const QString &name, uint32_t id, struct ::wl_resource *toplevel)
{
    XdgToplevelInterface* tl = nullptr;
    if (toplevel) {
        tl = XdgToplevelInterface::get(toplevel);
        if (!tl) {
            wl_resource_post_error(resource->handle, 0, "Invalid toplevel");
            return;
        }
    }

    auto annotation_resource = wl_resource_create(resource->client(), &xdg_dbus_annotation_manager_v1_interface, resource->version(), id);
    if (!annotation_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto annotation = new XdgDBusAnnotationInterface(tl ? XdgDBusAnnotationTarget(tl) : XdgDBusAnnotationTarget(resource->client()), name, resource->handle);
    annotations << annotation;
    QObject::connect(annotation, &QObject::destroyed, q, [this, annotation]() {
        annotations.removeOne(annotation);
    });
    Q_EMIT q->annotationCreated(annotation);
}

XdgDBusAnnotationInterface::Private::Private(XdgDBusAnnotationInterface *pub, const QString& name, const XdgDBusAnnotationTarget& target, wl_resource *resource)
    : QtWaylandServer::xdg_dbus_annotation_v1(resource)
    , q(pub)
    , name(name)
    , target(target)
{
}

void XdgDBusAnnotationInterface::Private::xdg_dbus_annotation_v1_destroy(QtWaylandServer::xdg_dbus_annotation_v1::Resource *resource)
{
    Q_UNUSED(resource)

    q->deleteLater();
}

void XdgDBusAnnotationInterface::Private::xdg_dbus_annotation_v1_set_address(Resource *resource, const QString &service_name, const QString &object_path)
{
    Q_UNUSED(resource)

    if (address.serviceName == service_name &&
            address.objectPath == object_path) {
        return;
    }

    address.serviceName = service_name;
    address.objectPath = object_path;

    Q_EMIT q->addressChanged();
}

};
