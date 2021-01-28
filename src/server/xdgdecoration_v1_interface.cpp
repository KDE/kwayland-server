/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgdecoration_v1_interface.h"
#include "display.h"
#include "xdgdecoration_v1_interface_p.h"
#include "xdgshell_interface_p.h"

namespace KWaylandServer
{
// TODO: We need to wait for an ack_configure either here or in xdgshellclient.cpp.

XdgDecorationManagerV1InterfacePrivate::XdgDecorationManagerV1InterfacePrivate(XdgDecorationManagerV1Interface *manager)
    : q(manager)
{
}

void XdgDecorationManagerV1InterfacePrivate::zxdg_decoration_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgDecorationManagerV1InterfacePrivate::zxdg_decoration_manager_v1_get_toplevel_decoration(Resource *resource,
                                                                                                uint32_t id,
                                                                                                ::wl_resource *toplevelResource)
{
    XdgToplevelInterfacePrivate *toplevelPrivate = XdgToplevelInterfacePrivate::get(toplevelResource);
    if (!toplevelPrivate) {
        wl_resource_post_error(resource->handle, QtWaylandServer::zxdg_toplevel_decoration_v1::error_orphaned, "no xdg-toplevel object");
        return;
    }

    if (toplevelPrivate->decoration) {
        wl_resource_post_error(resource->handle,
                               QtWaylandServer::zxdg_toplevel_decoration_v1::error_already_constructed,
                               "decoration has been already constructed");
        return;
    }

    wl_resource *decorationResource = wl_resource_create(resource->client(), &zxdg_toplevel_decoration_v1_interface, resource->version(), id);

    auto decoration = new XdgToplevelDecorationV1Interface(toplevelPrivate->q, decorationResource);
    toplevelPrivate->decoration = decoration;

    Q_EMIT q->decorationCreated(decoration);
}

XdgDecorationManagerV1Interface::XdgDecorationManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgDecorationManagerV1InterfacePrivate(this))
{
    d->init(*display, 1);
}

XdgDecorationManagerV1Interface::~XdgDecorationManagerV1Interface()
{
}

XdgToplevelDecorationV1InterfacePrivate::XdgToplevelDecorationV1InterfacePrivate(XdgToplevelDecorationV1Interface *decoration,
                                                                                 XdgToplevelInterface *toplevel)
    : q(decoration)
    , toplevel(toplevel)
{
    XdgSurfaceInterface *surface = toplevel->xdgSurface();

    QObject::connect(surface, &XdgSurfaceInterface::configureAcknowledged,
                     q, [this](quint32 serial) { handleSurfaceConfigureAcknowledged(serial); });
    QObject::connect(surface, &XdgSurfaceInterface::aboutToConfigure,
                     q, [this](quint32 serial) { handleSurfaceConfigure(serial); });
}

void XdgToplevelDecorationV1InterfacePrivate::zxdg_toplevel_decoration_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void XdgToplevelDecorationV1InterfacePrivate::zxdg_toplevel_decoration_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgToplevelDecorationV1InterfacePrivate::zxdg_toplevel_decoration_v1_set_mode(Resource *resource, uint32_t mode)
{
    Q_UNUSED(resource)

    switch (mode) {
    case mode_client_side:
        preferredMode = XdgToplevelDecorationV1Interface::Mode::Client;
        break;
    case mode_server_side:
        preferredMode = XdgToplevelDecorationV1Interface::Mode::Server;
        break;
    default:
        preferredMode = XdgToplevelDecorationV1Interface::Mode::Undefined;
        break;
    }

    Q_EMIT q->preferredModeChanged(preferredMode);
}

void XdgToplevelDecorationV1InterfacePrivate::zxdg_toplevel_decoration_v1_unset_mode(Resource *resource)
{
    Q_UNUSED(resource)
    preferredMode = XdgToplevelDecorationV1Interface::Mode::Undefined;
    Q_EMIT q->preferredModeChanged(preferredMode);
}

void XdgToplevelDecorationV1InterfacePrivate::handleSurfaceConfigure(quint32 serial)
{
    if (mode == requestedMode) {
        return;
    }

    switch (requestedMode) {
    case XdgToplevelDecorationV1Interface::Mode::Client:
        send_configure(QtWaylandServer::zxdg_toplevel_decoration_v1::mode_client_side);
        break;
    case XdgToplevelDecorationV1Interface::Mode::Server:
        send_configure(QtWaylandServer::zxdg_toplevel_decoration_v1::mode_server_side);
        break;
    case XdgToplevelDecorationV1Interface::Mode::Undefined:
        Q_UNREACHABLE();
    }

    configureEvents.append(XdgToplevelDecorationV1ConfigureEvent{requestedMode, serial});
}

void XdgToplevelDecorationV1InterfacePrivate::handleSurfaceConfigureAcknowledged(quint32 serial)
{
    while (!configureEvents.isEmpty()) {
        const XdgToplevelDecorationV1ConfigureEvent configure = configureEvents.takeFirst();
        if (configure.serial == serial) {
            mode = configure.mode;
            emit q->modeChanged(mode);
            return;
        }
    }
}

XdgToplevelDecorationV1Interface::XdgToplevelDecorationV1Interface(XdgToplevelInterface *toplevel, ::wl_resource *resource)
    : d(new XdgToplevelDecorationV1InterfacePrivate(this, toplevel))
{
    d->init(resource);
}

XdgToplevelDecorationV1Interface::~XdgToplevelDecorationV1Interface()
{
}

XdgToplevelInterface *XdgToplevelDecorationV1Interface::toplevel() const
{
    return d->toplevel;
}

XdgToplevelDecorationV1Interface::Mode XdgToplevelDecorationV1Interface::mode() const
{
    return d->mode;
}

XdgToplevelDecorationV1Interface::Mode XdgToplevelDecorationV1Interface::preferredMode() const
{
    return d->preferredMode;
}

void XdgToplevelDecorationV1Interface::scheduleConfigure(Mode mode)
{
    Q_ASSERT(mode != Mode::Undefined);
    d->requestedMode = mode;
}

XdgToplevelDecorationV1Interface *XdgToplevelDecorationV1Interface::get(XdgToplevelInterface *toplevel)
{
    XdgToplevelInterfacePrivate *toplevelPrivate = XdgToplevelInterfacePrivate::get(toplevel);
    return toplevelPrivate->decoration;
}

} // namespace KWaylandServer
