/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbufferref.h"
#include "clientbuffer_p.h"
#include "clientbufferintegration_p.h"

namespace KWaylandServer
{

class ClientBufferRefPrivate
{
public:
    ClientBuffer *buffer = nullptr;
};

ClientBufferRef::ClientBufferRef()
    : d(new ClientBufferRefPrivate)
{
}

ClientBufferRef::ClientBufferRef(ClientBufferRef &&other)
    : d(new ClientBufferRefPrivate)
{
    d->buffer = std::exchange(other.d->buffer, nullptr);
}

ClientBufferRef::ClientBufferRef(const ClientBufferRef &other)
    : d(new ClientBufferRefPrivate)
{
    d->buffer = other.d->buffer;
    if (d->buffer) {
        d->buffer->ref();
    }
}

ClientBufferRef::ClientBufferRef(ClientBuffer *buffer)
    : d(new ClientBufferRefPrivate)
{
    d->buffer = buffer;
    if (d->buffer) {
        d->buffer->ref();
    }
}

ClientBufferRef::~ClientBufferRef()
{
    if (d->buffer) {
        d->buffer->unref();
    }
}

ClientBufferRef &ClientBufferRef::operator=(ClientBufferRef &&other)
{
    if (this != &other) {
        if (d->buffer) {
            d->buffer->unref();
        }
        d->buffer = std::exchange(other.d->buffer, nullptr);
    }
    return *this;
}

ClientBufferRef &ClientBufferRef::operator=(const ClientBufferRef &other)
{
    if (other.d->buffer) {
        other.d->buffer->ref();
    }

    if (d->buffer) {
        d->buffer->unref();
    }

    d->buffer = other.d->buffer;

    return *this;
}

ClientBufferRef &ClientBufferRef::operator=(ClientBuffer *buffer)
{
    if (buffer) {
        buffer->ref();
    }

    if (d->buffer) {
        d->buffer->unref();
    }

    d->buffer = buffer;

    return *this;
}

bool ClientBufferRef::operator==(const ClientBufferRef &other) const
{
    return d->buffer == other.d->buffer;
}

bool ClientBufferRef::operator!=(const ClientBufferRef &other) const
{
    return !(*this == other);
}

ClientBufferRef::operator bool() const
{
    return d->buffer;
}

bool ClientBufferRef::isDestroyed() const
{
    if (d->buffer) {
        return d->buffer->isDestroyed();
    }
    return true;
}

bool ClientBufferRef::isNull() const
{
    return !d->buffer;
}

::wl_resource *ClientBufferRef::resource() const
{
    if (d->buffer) {
        return d->buffer->handle();
    }
    return nullptr;
}

ClientBufferRef::Origin ClientBufferRef::origin() const
{
    if (d->buffer) {
        return d->buffer->origin();
    }
    return Origin::TopLeft;
}

ClientBufferRef::TextureType ClientBufferRef::textureType() const
{
    if (d->buffer) {
        return d->buffer->textureType();
    }
    return TextureType::Unknown;
}

bool ClientBufferRef::hasAlphaChannel() const
{
    if (d->buffer) {
        return d->buffer->hasAlphaChannel();
    }
    return false;
}

QSize ClientBufferRef::size() const
{
    if (d->buffer) {
        return d->buffer->size();
    }
    return QSize();
}

QImage ClientBufferRef::toImage() const
{
    if (d->buffer) {
        return d->buffer->toImage();
    }
    return QImage();
}

GLuint ClientBufferRef::toOpenGLTexture(int plane) const
{
    if (d->buffer) {
        return d->buffer->toOpenGLTexture(plane);
    }
    return 0;
}

} // namespace KWaylandServer
