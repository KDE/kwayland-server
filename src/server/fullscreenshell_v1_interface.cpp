/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fullscreenshell_v1_interface.h"

#include "qwayland-server-fullscreen-shell-unstable-v1.h"
#include "surface_interface.h"
#include "surfacerole_p.h"

namespace KWaylandServer
{
class FullscreenShellModeFeedbackV1InterfacePrivate : public QtWaylandServer::zwp_fullscreen_shell_mode_feedback_v1, public SurfaceRole
{
public:
    FullscreenShellModeFeedbackV1InterfacePrivate(SurfaceInterface *surface,
                                                  OutputInterface *output,
                                                  int32_t framerate,
                                                  FullscreenShellModeFeedbackV1Interface *q)
        : SurfaceRole(surface, "zwp_fullscreen_shell_mode_feedback_v1")
        , m_surface(surface)
        , m_output(output)
        , m_framerate(framerate)
        , q(q)
    {
    }

    void commit() override {}

    SurfaceInterface *const m_surface;
    OutputInterface *const m_output;
    const int32_t m_framerate;
    FullscreenShellModeFeedbackV1Interface *const q;
};

FullscreenShellModeFeedbackV1Interface::FullscreenShellModeFeedbackV1Interface(SurfaceInterface *surface, OutputInterface *output, int32_t framerate)
    : d(new FullscreenShellModeFeedbackV1InterfacePrivate(surface, output, framerate, this))
{
}

FullscreenShellModeFeedbackV1Interface::~FullscreenShellModeFeedbackV1Interface() = default;

void FullscreenShellModeFeedbackV1Interface::sendModeSuccessful()
{
    d->send_mode_successful();
}

void FullscreenShellModeFeedbackV1Interface::sendModeFailed()
{
    d->send_mode_failed();
}

void FullscreenShellModeFeedbackV1Interface::sendPresentCancelled()
{
    d->send_present_cancelled();
}

SurfaceInterface *FullscreenShellModeFeedbackV1Interface::surface() const
{
    return d->m_surface;
}

OutputInterface *FullscreenShellModeFeedbackV1Interface::output() const
{
    return d->m_output;
}

int32_t FullscreenShellModeFeedbackV1Interface::framerate() const
{
    return d->m_framerate;
}

class FullscreenShellV1InterfacePrivate : public QtWaylandServer::zwp_fullscreen_shell_v1
{
public:
    FullscreenShellV1InterfacePrivate(FullscreenShellV1Interface *q, Display *d)
        : zwp_fullscreen_shell_v1(*d, 1)
        , q(q)
    {
    }

    void zwp_fullscreen_shell_v1_release(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void zwp_fullscreen_shell_v1_present_surface(Resource *resource, struct ::wl_resource *surface, uint32_t method, struct ::wl_resource *output) override;

    void zwp_fullscreen_shell_v1_present_surface_for_mode(Resource *resource,
                                                          struct ::wl_resource *_surface,
                                                          struct ::wl_resource *_output,
                                                          int32_t framerate,
                                                          uint32_t feedback) override;

    void zwp_fullscreen_shell_v1_bind_resource(Resource *resource) override
    {
        send_capability(resource->handle, m_capabilities);
    }

    FullscreenShellV1Interface *const q;
    FullscreenShellV1Interface::Capabilities m_capabilities;
};

FullscreenShellV1Interface::FullscreenShellV1Interface(Capabilities capabilities, Display *display, QObject *parent)
    : QObject(parent)
    , d(new FullscreenShellV1InterfacePrivate(this, display))
{
    d->m_capabilities = capabilities;
}

FullscreenShellV1Interface::~FullscreenShellV1Interface() = default;

struct FullScreenPresentedSurfaceRole : public QObject, public SurfaceRole
{
    FullScreenPresentedSurfaceRole(SurfaceInterface* surface)
        : SurfaceRole(surface, "zwp_fullscreen_shell_v1")
    {
        connect(surface, &QObject::destroyed, this, &QObject::deleteLater);
    }

    void commit() override {}
};

void FullscreenShellV1InterfacePrivate::zwp_fullscreen_shell_v1_present_surface(Resource *resource,
                                                                                struct ::wl_resource *surfaceResource,
                                                                                uint32_t method,
                                                                                struct ::wl_resource *output)
{
    Q_UNUSED(resource);
    auto surface = SurfaceInterface::get(surfaceResource);

    new FullScreenPresentedSurfaceRole(surface);

    Q_EMIT q->presentSurface(static_cast<FullscreenShellV1Interface::PresentMethod>(method), surface, OutputInterface::get(output));
}

void FullscreenShellV1InterfacePrivate::zwp_fullscreen_shell_v1_present_surface_for_mode(Resource *resource,
                                                                                         struct ::wl_resource *_surface,
                                                                                         struct ::wl_resource *_output,
                                                                                         int32_t framerate,
                                                                                         uint32_t feedback)
{
    auto iface = new FullscreenShellModeFeedbackV1Interface(SurfaceInterface::get(_surface), OutputInterface::get(_output), framerate);
    Q_ASSERT(*iface->d->m_surface->client() == resource->client());

    iface->d->add(resource->client(), feedback, interfaceVersion());

    QObject::connect(iface->d->m_surface, &QObject::destroyed, iface, &FullscreenShellModeFeedbackV1Interface::deleteLater);
}

}
