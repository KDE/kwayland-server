/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgdbusannotation_v1_interface_p.h"
#include "xdgshell_interface.h"

namespace KWaylandServer
{
XdgDBusAnnotationManagerV1InterfacePrivate::XdgDBusAnnotationManagerV1InterfacePrivate(XdgDBusAnnotationManagerV1Interface *pub, Display *display)
    : QtWaylandServer::xdg_dbus_annotation_manager_v1(*display, s_version)
    , q(pub)
    , display(display)
{
}

void XdgDBusAnnotationManagerV1InterfacePrivate::xdg_dbus_annotation_manager_v1_create(Resource *resource,
                                                                                       const QString &name,
                                                                                       uint32_t id,
                                                                                       struct ::wl_resource *toplevel_resource)
{
    XdgToplevelInterface *toplevel = nullptr;
    if (toplevel_resource) {
        toplevel = XdgToplevelInterface::get(toplevel_resource);
        if (!toplevel) {
            wl_resource_post_error(resource->handle, QtWaylandServer::xdg_dbus_annotation_manager_v1::error_bad_target, "Invalid toplevel");
            return;
        }
    }

    auto annotation_resource = wl_resource_create(resource->client(), &xdg_dbus_annotation_v1_interface, resource->version(), id);
    if (!annotation_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto target = toplevel ?
            XdgDBusAnnotationV1Target(toplevel) :
            XdgDBusAnnotationV1Target(display->getConnection(resource->client()));
    auto annotation = new XdgDBusAnnotationV1Interface(target, name, annotation_resource);
    annotations << annotation;
    QObject::connect(annotation, &QObject::destroyed, q, [this, annotation]() {
        annotations.removeOne(annotation);
    });
    Q_EMIT q->annotationCreated(annotation);
}

void XdgDBusAnnotationManagerV1InterfacePrivate::xdg_dbus_annotation_manager_v1_destroy(QtWaylandServer::xdg_dbus_annotation_manager_v1::Resource *resource)
{
    Q_UNUSED(resource)

    wl_resource_destroy(resource->handle);
    delete q;
}

XdgDBusAnnotationV1InterfacePrivate::XdgDBusAnnotationV1InterfacePrivate(XdgDBusAnnotationV1Interface *pub, const QString &name, const XdgDBusAnnotationV1Target &target, wl_resource *resource)
    : QtWaylandServer::xdg_dbus_annotation_v1(resource)
    , q(pub)
    , name(name)
    , target(target)
{
}

void XdgDBusAnnotationV1InterfacePrivate::xdg_dbus_annotation_v1_destroy(QtWaylandServer::xdg_dbus_annotation_v1::Resource *resource)
{
    Q_UNUSED(resource)

    wl_resource_destroy(resource->handle);
    delete q;
}

void XdgDBusAnnotationV1InterfacePrivate::xdg_dbus_annotation_v1_set_address(Resource *resource, const QString &service_name, const QString &object_path)
{
    Q_UNUSED(resource)

    if (address.serviceName == service_name && address.objectPath == object_path) {
        return;
    }

    address.serviceName = service_name;
    address.objectPath = object_path;

    Q_EMIT q->addressChanged();
}

};
