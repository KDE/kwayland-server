/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "rendererinterface.h"
#include "rendererinterface_p.h"
#include "clientbuffermanager_p.h"
#include "display.h"
#include "display_p.h"
#include "logging.h"

namespace KWaylandServer
{

RendererInterfacePrivate *RendererInterfacePrivate::get(RendererInterface *renderer)
{
    return renderer->d.data();
}

void RendererInterfacePrivate::addEventListener(RendererEventListener *listener)
{
    Q_ASSERT(!listeners.contains(listener));
    listeners.append(listener);
}

void RendererInterfacePrivate::removeEventListener(RendererEventListener *listener)
{
    Q_ASSERT(listeners.contains(listener));
    listeners.removeOne(listener);
}

void RendererInterfacePrivate::scheduleRemoveRenderbuffer(GLuint renderbuffer)
{
    m_zombieRenderbuffers.append(renderbuffer);
}

void RendererInterfacePrivate::scheduleRemoveTexture(GLuint texture)
{
    m_zombieTextures.append(texture);
}

void RendererInterfacePrivate::scheduleRemoveFramebuffer(GLuint framebuffer)
{
    m_zombieFramebuffers.append(framebuffer);
}

void RendererInterfacePrivate::destroyZombieResources()
{
    if (!m_zombieFramebuffers.isEmpty()) {
        glDeleteFramebuffers(m_zombieFramebuffers.count(), m_zombieFramebuffers.constData());
        m_zombieFramebuffers.clear();
    }

    if (!m_zombieTextures.isEmpty()) {
        glDeleteTextures(m_zombieTextures.count(), m_zombieTextures.constData());
        m_zombieTextures.clear();
    }

    if (!m_zombieRenderbuffers.isEmpty()) {
        glDeleteRenderbuffers(m_zombieRenderbuffers.count(), m_zombieRenderbuffers.constData());
        m_zombieRenderbuffers.clear();
    }
}

RendererInterface::RendererInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new RendererInterfacePrivate)
{
    d->display = display;
}

RendererInterface::~RendererInterface()
{
}

EGLDisplay RendererInterface::eglDisplay() const
{
    return d->eglDisplay;
}

void RendererInterface::setEglDisplay(EGLDisplay eglDisplay)
{
    if (d->eglDisplay == eglDisplay) {
        return;
    }

    if (!d->eglBindWaylandDisplayWL) {
        d->eglBindWaylandDisplayWL =
                reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
    }
    if (!d->eglUnbindWaylandDisplayWL) {
        d->eglUnbindWaylandDisplayWL =
                reinterpret_cast<PFNEGLUNBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglUnbindWaylandDisplayWL"));
    }

    if (!d->eglBindWaylandDisplayWL || !d->eglUnbindWaylandDisplayWL) {
        qCWarning(KWAYLAND_SERVER) << "Failed to load eglBindWaylandDisplayWL and eglUnbindWaylandDisplayWL";
        return;
    }

    if (d->eglDisplay) {
        if (!d->eglUnbindWaylandDisplayWL(d->eglDisplay, *d->display)) {
            qCWarning(KWAYLAND_SERVER, "Failed to unbind the wayland display: 0x%x", eglGetError());
        }
    }

    if (eglDisplay) {
        if (!d->eglBindWaylandDisplayWL(eglDisplay, *d->display)) {
            qCWarning(KWAYLAND_SERVER, "Failed to bind the wayland display: 0x%x", eglGetError());
        }
    }

    d->eglDisplay = eglDisplay;
}

RendererInterface::GraphicsApi RendererInterface::graphicsApi() const
{
    return d->graphicsApi;
}

void RendererInterface::setGraphicsApi(RendererInterface::GraphicsApi graphicsApi)
{
    d->graphicsApi = graphicsApi;
}

bool RendererInterface::supportsARGB32() const
{
    return d->supportsARGB32;
}

void RendererInterface::setSupportsARGB32(bool set)
{
    d->supportsARGB32 = set;
}

void RendererInterface::invalidateGraphics()
{
    for (RendererEventListener *listener : qAsConst(d->listeners)) {
        listener->handleGraphicsReset();
    }
    d->destroyZombieResources();
}

} // namespace KWaylandServer
