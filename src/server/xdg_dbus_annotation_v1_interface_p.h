/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "display.h"
#include "surface_interface.h"
#include "xdg_dbus_annotation_v1_interface.h"

#include "qwayland-server-xdg-dbus-annotation-v1.h"

namespace KWaylandServer
{

static constexpr quint32 s_version = 1;

struct XdgDBusAnnotationManagerInterface::Private : public QtWaylandServer::xdg_dbus_annotation_manager_v1
{
    Private(XdgDBusAnnotationManagerInterface *pub, Display *display);

    QVector<XdgDBusAnnotationInterface*> annotations;
    XdgDBusAnnotationManagerInterface *q;

protected:
    void xdg_dbus_annotation_manager_v1_create(Resource *resource, const QString &name, uint32_t id, struct ::wl_resource *toplevel) override;
};

struct XdgDBusAnnotationInterface::Private : public QtWaylandServer::xdg_dbus_annotation_v1
{

    Private(XdgDBusAnnotationInterface *pub, const QString& name, const XdgDBusAnnotationTarget& target, wl_resource *resource);

    XdgDBusAnnotationInterface *q;
    QString name;
    XdgDBusAnnotationTarget target;
    InterfaceAddress address;

protected:
    void xdg_dbus_annotation_v1_destroy(Resource *resource) override;
    void xdg_dbus_annotation_v1_set_address(Resource *resource, const QString &service_name, const QString &object_path) override;
};

};
