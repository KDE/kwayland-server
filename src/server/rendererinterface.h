/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

namespace KWaylandServer
{

class Display;
class RendererInterfacePrivate;

typedef void *EGLDisplay;

/**
 * The RendererInterface class provides an interface between the compositor's renderer and
 * the rest of kwaylandserver.
 */
class KWAYLANDSERVER_EXPORT RendererInterface : public QObject
{
    Q_OBJECT

public:
    explicit RendererInterface(Display *display, QObject *parent = nullptr);
    ~RendererInterface() override;

    enum class GraphicsApi {
        OpenGL,
        OpenGLES,
        QPainter,
    };

    /**
     * Returns the renderer's EGLDisplay object.
     */
    EGLDisplay eglDisplay() const;
    /**
     * Sets the EGLDisplay to @a eglDisplay. If the specified EGL display is valid, it will be
     * bound to the Wayland display.
     */
    void setEglDisplay(EGLDisplay eglDisplay);

    /**
     * Returns the graphics api used by the renderer.
     */
    GraphicsApi graphicsApi() const;

    /**
     * Sets the graphics api used by the renderer to @a graphicsApi.
     */
    void setGraphicsApi(GraphicsApi graphicsApi);

    /**
     * Returns @c true if the renderer supports efficient uploading of images with a format
     * of QImage::Format_ARGB32 or QImage::Format_ARGB32_Premultiplied to textures.
     */
    bool supportsARGB32() const;
    void setSupportsARGB32(bool set);

    /**
     * The compositor has to call this function when the OpenGL context has been lost so the
     * textures for client buffers can be properly destroyed.
     */
    void invalidateGraphics();

private:
    QScopedPointer<RendererInterfacePrivate> d;
    friend class RendererInterfacePrivate;
};

} // namespace KWaylandServer
