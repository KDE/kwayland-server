/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwaylandeglext.h"
#include "rendererinterface.h"

namespace KWaylandServer
{

//
//  W A R N I N G
//  -------------
//
// This file is part of the KWaylandServer API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of KWaylandServer.
//

class KWAYLANDSERVER_EXPORT RendererEventListener
{
public:
    virtual ~RendererEventListener() {}

protected:
    virtual void handleGraphicsReset() {}

private:
    friend class RendererInterface;
};

class KWAYLANDSERVER_EXPORT RendererInterfacePrivate
{
public:
    static RendererInterfacePrivate *get(RendererInterface *renderer);

    void addEventListener(RendererEventListener *listener);
    void removeEventListener(RendererEventListener *listener);

    void scheduleRemoveRenderbuffer(GLuint renderbuffer);
    void scheduleRemoveTexture(GLuint texture);
    void scheduleRemoveFramebuffer(GLuint framebuffer);

    void destroyZombieResources();

    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    Display *display;
    RendererInterface::GraphicsApi graphicsApi;
    PFNEGLBINDWAYLANDDISPLAYWL eglBindWaylandDisplayWL = nullptr;
    PFNEGLUNBINDWAYLANDDISPLAYWL eglUnbindWaylandDisplayWL = nullptr;
    QList<RendererEventListener *> listeners;
    QVector<GLuint> m_zombieRenderbuffers;
    QVector<GLuint> m_zombieTextures;
    QVector<GLuint> m_zombieFramebuffers;
    bool supportsARGB32 = false;
};

} // namespace KWaylandServer
