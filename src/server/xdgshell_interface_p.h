/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "xdgshell_interface.h"
#include "qwayland-server-xdg-shell.h"

#include "keyboardgrab.h"
#include "pointergrab.h"
#include "surface_interface.h"
#include "surfacerole_p.h"
#include "touchgrab.h"

#include <QStack>

namespace KWaylandServer
{

class XdgPopupGrab;
class XdgToplevelDecorationV1Interface;

class XdgShellInterfacePrivate : public QtWaylandServer::xdg_wm_base
{
public:
    XdgShellInterfacePrivate(XdgShellInterface *shell);

    Resource *resourceForXdgSurface(XdgSurfaceInterface *surface) const;

    void registerXdgSurface(XdgSurfaceInterface *surface);
    void unregisterXdgSurface(XdgSurfaceInterface *surface);

    XdgPopupGrab *getOrCreatePopupGrab(SeatInterface *seat, ClientConnection *client);
    void destroyPopupGrab(XdgPopupGrab *grab);

    void registerPing(quint32 serial);

    static XdgShellInterfacePrivate *get(XdgShellInterface *shell);

    XdgShellInterface *q;
    Display *display;
    QMap<quint32, QTimer *> pings;
    QList<XdgPopupGrab *> grabs;

protected:
    void xdg_wm_base_destroy(Resource *resource) override;
    void xdg_wm_base_create_positioner(Resource *resource, uint32_t id) override;
    void xdg_wm_base_get_xdg_surface(Resource *resource, uint32_t id, ::wl_resource *surface) override;
    void xdg_wm_base_pong(Resource *resource, uint32_t serial) override;

private:
    QMultiMap<wl_client *, XdgSurfaceInterface *> xdgSurfaces;
};

class XdgPositionerData : public QSharedData
{
public:
    Qt::Orientations slideConstraintAdjustments;
    Qt::Orientations flipConstraintAdjustments;
    Qt::Orientations resizeConstraintAdjustments;
    Qt::Edges anchorEdges;
    Qt::Edges gravityEdges;
    QPoint offset;
    QSize size;
    QRect anchorRect;
    bool isReactive = false;
    QSize parentSize;
    quint32 parentConfigure;
};

class XdgPositionerPrivate : public QtWaylandServer::xdg_positioner
{
public:
    XdgPositionerPrivate(::wl_resource *resource);

    QSharedDataPointer<XdgPositionerData> data;

    static XdgPositionerPrivate *get(::wl_resource *resource);

protected:
    void xdg_positioner_destroy_resource(Resource *resource) override;
    void xdg_positioner_destroy(Resource *resource) override;
    void xdg_positioner_set_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_positioner_set_anchor_rect(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void xdg_positioner_set_anchor(Resource *resource, uint32_t anchor) override;
    void xdg_positioner_set_gravity(Resource *resource, uint32_t gravity) override;
    void xdg_positioner_set_constraint_adjustment(Resource *resource, uint32_t constraint_adjustment) override;
    void xdg_positioner_set_offset(Resource *resource, int32_t x, int32_t y) override;
    void xdg_positioner_set_reactive(Resource *resource) override;
    void xdg_positioner_set_parent_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_positioner_set_parent_configure(Resource *resource, uint32_t serial) override;
};

class XdgSurfaceInterfacePrivate : public QtWaylandServer::xdg_surface
{
public:
    XdgSurfaceInterfacePrivate(XdgSurfaceInterface *xdgSurface);

    void commit();
    void reset();

    XdgSurfaceInterface *q;
    XdgShellInterface *shell;
    QPointer<XdgToplevelInterface> toplevel;
    QPointer<XdgPopupInterface> popup;
    QPointer<SurfaceInterface> surface;
    bool isMapped = false;
    bool isConfigured = false;

    struct State
    {
        QRect windowGeometry;
    };

    State next;
    State current;

    static XdgSurfaceInterfacePrivate *get(XdgSurfaceInterface *surface);

protected:
    void xdg_surface_destroy_resource(Resource *resource) override;
    void xdg_surface_destroy(Resource *resource) override;
    void xdg_surface_get_toplevel(Resource *resource, uint32_t id) override;
    void xdg_surface_get_popup(Resource *resource, uint32_t id, ::wl_resource *parent, ::wl_resource *positioner) override;
    void xdg_surface_set_window_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void xdg_surface_ack_configure(Resource *resource, uint32_t serial) override;
};

class XdgToplevelInterfacePrivate : public SurfaceRole, public QtWaylandServer::xdg_toplevel
{
public:
    XdgToplevelInterfacePrivate(XdgToplevelInterface *toplevel, XdgSurfaceInterface *surface);

    void commit() override;
    void reset();

    static XdgToplevelInterfacePrivate *get(XdgToplevelInterface *toplevel);
    static XdgToplevelInterfacePrivate *get(::wl_resource *resource);

    XdgToplevelInterface *q;
    QPointer<XdgToplevelInterface> parentXdgToplevel;
    QPointer<XdgToplevelDecorationV1Interface> decoration;
    XdgSurfaceInterface *xdgSurface;

    QString windowTitle;
    QString windowClass;

    struct State
    {
        QSize minimumSize;
        QSize maximumSize;
    };

    State next;
    State current;

protected:
    void xdg_toplevel_destroy_resource(Resource *resource) override;
    void xdg_toplevel_destroy(Resource *resource) override;
    void xdg_toplevel_set_parent(Resource *resource, ::wl_resource *parent) override;
    void xdg_toplevel_set_title(Resource *resource, const QString &title) override;
    void xdg_toplevel_set_app_id(Resource *resource, const QString &app_id) override;
    void xdg_toplevel_show_window_menu(Resource *resource, ::wl_resource *seat, uint32_t serial, int32_t x, int32_t y) override;
    void xdg_toplevel_move(Resource *resource, ::wl_resource *seat, uint32_t serial) override;
    void xdg_toplevel_resize(Resource *resource, ::wl_resource *seat, uint32_t serial, uint32_t edges) override;
    void xdg_toplevel_set_max_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_toplevel_set_min_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_toplevel_set_maximized(Resource *resource) override;
    void xdg_toplevel_unset_maximized(Resource *resource) override;
    void xdg_toplevel_set_fullscreen(Resource *resource, ::wl_resource *output) override;
    void xdg_toplevel_unset_fullscreen(Resource *resource) override;
    void xdg_toplevel_set_minimized(Resource *resource) override;
};

class XdgPopupInterfacePrivate : public SurfaceRole, public QtWaylandServer::xdg_popup
{
public:
    static XdgPopupInterfacePrivate *get(XdgPopupInterface *popup);

    XdgPopupInterfacePrivate(XdgPopupInterface *popup, XdgSurfaceInterface *surface);

    void commit() override;
    void reset();

    XdgPopupInterface *q;
    SurfaceInterface *parentSurface;
    XdgSurfaceInterface *xdgSurface;
    XdgPositioner positioner;
    XdgPopupGrab *popupGrab = nullptr;

protected:
    void xdg_popup_destroy_resource(Resource *resource) override;
    void xdg_popup_destroy(Resource *resource) override;
    void xdg_popup_grab(Resource *resource, ::wl_resource *seat, uint32_t serial) override;
    void xdg_popup_reposition(Resource *resource, struct ::wl_resource *positioner, uint32_t token) override;
};

class XdgPopupKeyboardGrab : public KeyboardGrab
{
    Q_OBJECT

public:
    XdgPopupKeyboardGrab(XdgPopupGrab *grab, SeatInterface *seat);

    void cancel() override;

    void handleFocusChange(SurfaceInterface *surface, quint32 serial) override;
    void handlePressEvent(quint32 keyCode) override;
    void handleReleaseEvent(quint32 keyCode) override;
    void handleModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group) override;

private:
    XdgPopupGrab *m_popupGrab;
};

class XdgPopupTouchGrab : public TouchGrab
{
    Q_OBJECT

public:
    XdgPopupTouchGrab(XdgPopupGrab *grab, SeatInterface *seat);

    void cancel() override;

    void handleFocusChange(SurfaceInterface *surface) override;
    void handleDown(qint32 id, quint32 serial, const QPointF &localPos) override;
    void handleUp(qint32 id, quint32 serial) override;
    void handleFrame() override;
    void handleCancel() override;
    void handleMotion(qint32 id, const QPointF &localPos) override;

private:
    XdgPopupGrab *m_popupGrab;
};

class XdgPopupPointerGrab : public PointerGrab
{
    Q_OBJECT

public:
    XdgPopupPointerGrab(XdgPopupGrab *grab, SeatInterface *seat);

    void cancel() override;

    void handleFocusChange(SurfaceInterface *surface, const QPointF &position, quint32 serial) override;
    void handlePressed(quint32 button, quint32 serial) override;
    void handleReleased(quint32 button, quint32 serial) override;
    void handleAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source) override;
    void handleMotion(const QPointF &position) override;
    void handleFrame() override;

private:
    XdgPopupGrab *m_popupGrab;
};

class XdgPopupGrab
{
public:
    explicit XdgPopupGrab(SeatInterface *seat, ClientConnection *client);
    ~XdgPopupGrab();

    void addPopup(XdgPopupInterface *popup);
    void removePopup(XdgPopupInterface *popup);

    SeatInterface *seat() const;
    ClientConnection *client() const;

    void cancel();

    QStack<XdgPopupInterface *> stack;
    QScopedPointer<XdgPopupPointerGrab> pointerGrab;
    QScopedPointer<XdgPopupKeyboardGrab> keyboardGrab;
    QScopedPointer<XdgPopupTouchGrab> touchGrab;
    ClientConnection *m_client;
};

} // namespace KWaylandServer
