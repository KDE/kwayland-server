/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "display.h"
#include "surface_interface.h"
#include "xdgdbusannotation_v1_interface.h"

#include "qwayland-server-xdg-dbus-annotation-v1.h"

namespace KWaylandServer
{
static constexpr quint32 s_version = 1;

class XdgDBusAnnotationManagerV1InterfacePrivate : public QtWaylandServer::xdg_dbus_annotation_manager_v1 {
public:
    XdgDBusAnnotationManagerV1InterfacePrivate(XdgDBusAnnotationManagerV1Interface *pub, Display *display);

    QVector<XdgDBusAnnotationV1Interface *> annotations;
    XdgDBusAnnotationManagerV1Interface *q;
    Display *display;

protected:
    void xdg_dbus_annotation_manager_v1_create(Resource *resource, const QString &name, uint32_t id, struct ::wl_resource *toplevel) override;
    void xdg_dbus_annotation_manager_v1_destroy(Resource *resource) override;
};

class XdgDBusAnnotationV1InterfacePrivate : public QtWaylandServer::xdg_dbus_annotation_v1 {
public:
    XdgDBusAnnotationV1InterfacePrivate(XdgDBusAnnotationV1Interface *pub, const QString &name, const XdgDBusAnnotationV1Target &target, wl_resource *resource);

    XdgDBusAnnotationV1Interface *q;
    QString name;
    XdgDBusAnnotationV1Target target;
    XdgDBusAnnotationV1Interface::InterfaceAddress address;

protected:
    void xdg_dbus_annotation_v1_destroy(Resource *resource) override;
    void xdg_dbus_annotation_v1_set_address(Resource *resource, const QString &service_name, const QString &object_path) override;
};

};
