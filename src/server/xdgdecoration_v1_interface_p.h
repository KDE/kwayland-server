/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "xdgdecoration_v1_interface.h"

#include "qwayland-server-xdg-decoration-unstable-v1.h"

namespace KWaylandServer
{
class XdgDecorationManagerV1InterfacePrivate : public QtWaylandServer::zxdg_decoration_manager_v1
{
public:
    XdgDecorationManagerV1InterfacePrivate(XdgDecorationManagerV1Interface *manager);

    XdgDecorationManagerV1Interface *q;

protected:
    void zxdg_decoration_manager_v1_destroy(Resource *resource) override;
    void zxdg_decoration_manager_v1_get_toplevel_decoration(Resource *resource, uint32_t id, ::wl_resource *toplevel) override;
};

class XdgToplevelDecorationV1ConfigureEvent
{
public:
    XdgToplevelDecorationV1Interface::Mode mode;
    quint32 serial;
};

class XdgToplevelDecorationV1InterfacePrivate : public QtWaylandServer::zxdg_toplevel_decoration_v1
{
public:
    XdgToplevelDecorationV1InterfacePrivate(XdgToplevelDecorationV1Interface *decoration,
                                            XdgToplevelInterface *toplevel);

    void handleSurfaceConfigure(quint32 serial);
    void handleSurfaceConfigureAcknowledged(quint32 serial);

    XdgToplevelDecorationV1Interface *q;
    XdgToplevelInterface *toplevel;
    XdgToplevelDecorationV1Interface::Mode mode = XdgToplevelDecorationV1Interface::Mode::Undefined;
    XdgToplevelDecorationV1Interface::Mode requestedMode = XdgToplevelDecorationV1Interface::Mode::Undefined;
    XdgToplevelDecorationV1Interface::Mode preferredMode = XdgToplevelDecorationV1Interface::Mode::Undefined;
    QVector<XdgToplevelDecorationV1ConfigureEvent> configureEvents;

protected:
    void zxdg_toplevel_decoration_v1_destroy_resource(Resource *resource) override;
    void zxdg_toplevel_decoration_v1_destroy(Resource *resource) override;
    void zxdg_toplevel_decoration_v1_set_mode(Resource *resource, uint32_t mode) override;
    void zxdg_toplevel_decoration_v1_unset_mode(Resource *resource) override;
};

} // namespace KWaylandServer
