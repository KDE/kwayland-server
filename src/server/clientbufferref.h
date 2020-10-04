/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QImage>
#include <QScopedPointer>
#include <QSize>

struct wl_resource;

typedef unsigned int GLuint;

namespace KWaylandServer
{

class ClientBuffer;
class ClientBufferRefPrivate;
class ClientBufferRef;

/**
 * The ClientBufferRef class represents a reference to a Wayland client buffer.
 *
 * The ClientBufferRef is not the actual client buffer, but rather a reference to it. While
 * the buffer is referenced, the client cannot modify it. The buffer will be automatically
 * released once all corresponding ClientBufferRef objects go out of scope. After a buffer
 * has been released, the client can re-use it or destroy it.
 */
class KWAYLANDSERVER_EXPORT ClientBufferRef
{
public:
    enum class Origin {
        TopLeft,
        BottomLeft,
    };

    enum class TextureType {
        Unknown,
        RGBA,
        Y_XUXV,
        Y_UV,
        Y_U_V,
        XYUV,
    };

    ClientBufferRef();
    ClientBufferRef(ClientBufferRef &&other);
    ClientBufferRef(const ClientBufferRef &other);
    ClientBufferRef(ClientBuffer *buffer);
    ~ClientBufferRef();

    ClientBufferRef &operator=(ClientBufferRef &&other);
    ClientBufferRef &operator=(const ClientBufferRef &other);
    ClientBufferRef &operator=(ClientBuffer *buffer);

    bool operator==(const ClientBufferRef &other) const;
    bool operator!=(const ClientBufferRef &other) const;
    operator bool() const;

    /**
     * Returns @c true if the underlying resource has been destroyed; otherwise returns @c false.
     */
    bool isDestroyed() const;
    /**
     * Returns @c true if the reference doesn't reference any buffer; otherwise returns @c false.
     */
    bool isNull() const;

    /**
     * Returns the position of the origin of the client buffer.
     */
    Origin origin() const;
    /**
     * Returns the texture type for this client buffer. If the buffer reference is null, this
     * function returns TextureType::Unknown.
     */
    TextureType textureType() const;
    /**
     * Returns @c true if the client buffer has an alpha channel; otherwise returns @c false.
     */
    bool hasAlphaChannel() const;
    /**
     * Returns the size of the client buffer, in native pixels.
     *
     * Note that the returned value is not affected by the buffer scale or viewporter transform.
     */
    QSize size() const;
    /**
     * Returns the OpenGL texture for the specified @a plane in the client buffer.
     *
     * It is important to have a current OpenGL context set when calling this function or else
     * the compositor will crash.
     *
     * If the referenced shared memory client buffer has been destroyed, the attach operation
     * may fail.
     */
    GLuint toOpenGLTexture(int plane) const;
    /**
     * Returns the contents of the client buffer as a QImage object. If the client buffer has
     * been destroyed or it's not a shm buffer, this function will return an invalid QImage.
     */
    QImage toImage() const;

    /**
     * Returns the underlying wl_resource object for the client buffer. This functions returns
     * @c null if the client buffer reference is null.
     */
    ::wl_resource *resource() const;

private:
    QScopedPointer<ClientBufferRefPrivate> d;
};

} // namespace KWaylandServer
