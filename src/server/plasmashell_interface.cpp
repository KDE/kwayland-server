/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "plasmashell_interface.h"
#include "display.h"
#include "surface_interface.h"
#include "utils.h"

#include <qwayland-server-plasma-shell.h>

namespace KWaylandServer
{
static const quint32 s_version = 6;
static QList<PlasmaShellSurfaceInterface *> s_shellSurfaces;

class PlasmaShellInterfacePrivate : public QtWaylandServer::org_kde_plasma_shell
{
public:
    PlasmaShellInterfacePrivate(PlasmaShellInterface *q, Display *display);

private:
    void org_kde_plasma_shell_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    PlasmaShellInterface *q;
};

PlasmaShellInterfacePrivate::PlasmaShellInterfacePrivate(PlasmaShellInterface *_q, Display *display)
    : QtWaylandServer::org_kde_plasma_shell(*display, s_version)
    , q(_q)
{
}

class PlasmaShellSurfaceInterfacePrivate : public QtWaylandServer::org_kde_plasma_surface
{
public:
    PlasmaShellSurfaceInterfacePrivate(PlasmaShellSurfaceInterface *q, SurfaceInterface *surface, wl_resource *resource);

    QPointer<SurfaceInterface> surface;
    PlasmaShellSurfaceInterface *q;
    QPoint m_globalPos;
    PlasmaShellSurfaceInterface::Role m_role = PlasmaShellSurfaceInterface::Role::Normal;
    PlasmaShellSurfaceInterface::PanelBehavior m_panelBehavior = PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible;
    bool m_positionSet = false;
    bool m_skipTaskbar = false;
    bool m_skipSwitcher = false;
    bool m_panelTakesFocus = false;
    bool m_openUnderCursorRequested = false;

private:
    void org_kde_plasma_surface_destroy_resource(Resource *resource) override;
    void org_kde_plasma_surface_destroy(Resource *resource) override;
    void org_kde_plasma_surface_set_output(Resource *resource, struct ::wl_resource *output) override;
    void org_kde_plasma_surface_set_position(Resource *resource, int32_t x, int32_t y) override;
    void org_kde_plasma_surface_set_role(Resource *resource, uint32_t role) override;
    void org_kde_plasma_surface_set_panel_behavior(Resource *resource, uint32_t flag) override;
    void org_kde_plasma_surface_set_skip_taskbar(Resource *resource, uint32_t skip) override;
    void org_kde_plasma_surface_panel_auto_hide_hide(Resource *resource) override;
    void org_kde_plasma_surface_panel_auto_hide_show(Resource *resource) override;
    void org_kde_plasma_surface_set_panel_takes_focus(Resource *resource, uint32_t takes_focus) override;
    void org_kde_plasma_surface_set_skip_switcher(Resource *resource, uint32_t skip) override;
    void org_kde_plasma_surface_open_under_cursor(Resource *resource) override;
};

PlasmaShellInterface::PlasmaShellInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PlasmaShellInterfacePrivate(this, display))
{
}

PlasmaShellInterface::~PlasmaShellInterface() = default;

void PlasmaShellInterfacePrivate::org_kde_plasma_shell_get_surface(QtWaylandServer::org_kde_plasma_shell::Resource *resource,
                                                                   uint32_t id,
                                                                   struct ::wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    if (PlasmaShellSurfaceInterface::get(s)) {
        wl_resource_post_error(resource->handle, 0, "org_kde_plasma_shell_surface already exists");
        return;
    }

    wl_resource *shell_resource = wl_resource_create(resource->client(), &org_kde_plasma_surface_interface, resource->version(), id);

    auto shellSurface = new PlasmaShellSurfaceInterface(s, shell_resource);
    s_shellSurfaces.append(shellSurface);

    QObject::connect(shellSurface, &QObject::destroyed, [shellSurface]() {
        s_shellSurfaces.removeOne(shellSurface);
    });

    Q_EMIT q->surfaceCreated(shellSurface);
}

/*********************************
 * ShellSurfaceInterface
 *********************************/
PlasmaShellSurfaceInterfacePrivate::PlasmaShellSurfaceInterfacePrivate(PlasmaShellSurfaceInterface *_q, SurfaceInterface *surface, wl_resource *resource)
    : QtWaylandServer::org_kde_plasma_surface(resource)
    , surface(surface)
    , q(_q)
{
}

PlasmaShellSurfaceInterface::PlasmaShellSurfaceInterface(SurfaceInterface *surface, wl_resource *resource)
    : d(new PlasmaShellSurfaceInterfacePrivate(this, surface, resource))
{
}

PlasmaShellSurfaceInterface::~PlasmaShellSurfaceInterface() = default;

SurfaceInterface *PlasmaShellSurfaceInterface::surface() const
{
    return d->surface;
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_output(Resource *resource, struct ::wl_resource *output)
{
    Q_UNUSED(resource)
    Q_UNUSED(output)
    // TODO: implement
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_position(Resource *resource, int32_t x, int32_t y)
{
    Q_UNUSED(resource);
    QPoint globalPos(x, y);
    if (m_globalPos == globalPos && m_positionSet) {
        return;
    }
    m_positionSet = true;
    m_globalPos = globalPos;
    Q_EMIT q->positionChanged();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_open_under_cursor(Resource *resource)
{
    if (surface && surface->buffer()) {
        wl_resource_post_error(resource->handle, -1, "open_under_cursor: surface has a buffer");
    }
    m_openUnderCursorRequested = true;
    Q_EMIT q->openUnderCursorRequested();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_role(Resource *resource, uint32_t role)
{
    Q_UNUSED(resource)

    PlasmaShellSurfaceInterface::Role r = PlasmaShellSurfaceInterface::Role::Normal;
    switch (role) {
    case role_desktop:
        r = PlasmaShellSurfaceInterface::Role::Desktop;
        break;
    case role_panel:
        r = PlasmaShellSurfaceInterface::Role::Panel;
        break;
    case role_onscreendisplay:
        r = PlasmaShellSurfaceInterface::Role::OnScreenDisplay;
        break;
    case role_notification:
        r = PlasmaShellSurfaceInterface::Role::Notification;
        break;
    case role_tooltip:
        r = PlasmaShellSurfaceInterface::Role::ToolTip;
        break;
    case role_criticalnotification:
        r = PlasmaShellSurfaceInterface::Role::CriticalNotification;
        break;
    case role_normal:
    default:
        r = PlasmaShellSurfaceInterface::Role::Normal;
        break;
    }
    if (r == m_role) {
        return;
    }
    m_role = r;
    Q_EMIT q->roleChanged();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_panel_behavior(Resource *resource, uint32_t flag)
{
    Q_UNUSED(resource)

    PlasmaShellSurfaceInterface::PanelBehavior newBehavior = PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible;
    switch (flag) {
    case panel_behavior_auto_hide:
        newBehavior = PlasmaShellSurfaceInterface::PanelBehavior::AutoHide;
        break;
    case panel_behavior_windows_can_cover:
        newBehavior = PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover;
        break;
    case panel_behavior_windows_go_below:
        newBehavior = PlasmaShellSurfaceInterface::PanelBehavior::WindowsGoBelow;
        break;
    case panel_behavior_always_visible:
    default:
        break;
    }
    if (m_panelBehavior == newBehavior) {
        return;
    }
    m_panelBehavior = newBehavior;
    Q_EMIT q->panelBehaviorChanged();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_skip_taskbar(Resource *resource, uint32_t skip)
{
    Q_UNUSED(resource)

    m_skipTaskbar = (bool)skip;
    Q_EMIT q->skipTaskbarChanged();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_skip_switcher(Resource *resource, uint32_t skip)
{
    Q_UNUSED(resource)

    m_skipSwitcher = (bool)skip;
    Q_EMIT q->skipSwitcherChanged();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_panel_auto_hide_hide(Resource *resource)
{
    if (m_role != PlasmaShellSurfaceInterface::Role::Panel
        || (m_panelBehavior != PlasmaShellSurfaceInterface::PanelBehavior::AutoHide
            && m_panelBehavior != PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover)) {
        wl_resource_post_error(resource->handle, error_panel_not_auto_hide, "Not an auto hide panel");
        return;
    }
    Q_EMIT q->panelAutoHideHideRequested();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_panel_auto_hide_show(Resource *resource)
{
    if (m_role != PlasmaShellSurfaceInterface::Role::Panel || m_panelBehavior != PlasmaShellSurfaceInterface::PanelBehavior::AutoHide) {
        wl_resource_post_error(resource->handle, error_panel_not_auto_hide, "Not an auto hide panel");
        return;
    }
    Q_EMIT q->panelAutoHideShowRequested();
}

void PlasmaShellSurfaceInterfacePrivate::org_kde_plasma_surface_set_panel_takes_focus(Resource *resource, uint32_t takesFocus)
{
    Q_UNUSED(resource)

    if (m_panelTakesFocus == takesFocus) {
        return;
    }
    m_panelTakesFocus = takesFocus;
    Q_EMIT q->panelTakesFocusChanged();
}

QPoint PlasmaShellSurfaceInterface::position() const
{
    return d->m_globalPos;
}

PlasmaShellSurfaceInterface::Role PlasmaShellSurfaceInterface::role() const
{
    return d->m_role;
}

bool PlasmaShellSurfaceInterface::isPositionSet() const
{
    return d->m_positionSet;
}

bool PlasmaShellSurfaceInterface::wantsOpenUnderCursor() const
{
    return d->m_openUnderCursorRequested;
}

PlasmaShellSurfaceInterface::PanelBehavior PlasmaShellSurfaceInterface::panelBehavior() const
{
    return d->m_panelBehavior;
}

bool PlasmaShellSurfaceInterface::skipTaskbar() const
{
    return d->m_skipTaskbar;
}

bool PlasmaShellSurfaceInterface::skipSwitcher() const
{
    return d->m_skipSwitcher;
}

void PlasmaShellSurfaceInterface::hideAutoHidingPanel()
{
    d->send_auto_hidden_panel_hidden();
}

void PlasmaShellSurfaceInterface::showAutoHidingPanel()
{
    d->send_auto_hidden_panel_shown();
}

bool PlasmaShellSurfaceInterface::panelTakesFocus() const
{
    return d->m_panelTakesFocus;
}

PlasmaShellSurfaceInterface *PlasmaShellSurfaceInterface::get(wl_resource *native)
{
    if (auto surfacePrivate = resource_cast<PlasmaShellSurfaceInterfacePrivate *>(native)) {
        return surfacePrivate->q;
    }
    return nullptr;
}

PlasmaShellSurfaceInterface *PlasmaShellSurfaceInterface::get(SurfaceInterface *surface)
{
    for (PlasmaShellSurfaceInterface *shellSurface : qAsConst(s_shellSurfaces)) {
        if (shellSurface->surface() == surface) {
            return shellSurface;
        }
    }
    return nullptr;
}

}
