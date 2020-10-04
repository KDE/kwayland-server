/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbuffer_p.h"
#include "clientbufferintegration_p.h"
#include "utils.h"

#include <wayland-server-protocol.h>

namespace KWaylandServer
{

ClientBuffer::ClientBuffer(ClientBufferIntegration *integration, wl_resource *resource)
    : QObject(integration)
    , m_damageRegion(infiniteRegion())
    , m_handle(resource)
    , m_integration(integration)
{
}

QImage ClientBuffer::toImage()
{
    return QImage();
}

GLuint ClientBuffer::toOpenGLTexture(int plane)
{
    Q_UNUSED(plane)
    return 0;
}

::wl_resource *ClientBuffer::handle() const
{
    return m_handle;
}

ClientBufferIntegration *ClientBuffer::integration() const
{
    return m_integration;
}

bool ClientBuffer::isReferenced() const
{
    return m_refCount > 0;
}

bool ClientBuffer::isDestroyed() const
{
    return !m_handle;
}

void ClientBuffer::ref()
{
    m_refCount++;
}

void ClientBuffer::unref()
{
    Q_ASSERT_X(isReferenced(), "unref", "Buffer must be referenced");
    m_refCount--;

    if (!isReferenced()) {
        if (isDestroyed()) {
            delete this;
        } else {
            wl_buffer_send_release(m_handle);
        }
    }
}

void ClientBuffer::markAsDestroyed()
{
    if (!isReferenced()) {
        delete this;
    } else {
        m_handle = nullptr;
    }
}

void ClientBuffer::markAsDirty(const QRegion &region, quint64 cursor, quint64 handle)
{
    m_damageRegion += region;
    m_damageCursor = cursor;
    m_damageHandle = handle;
}

quint64 ClientBuffer::damageCursor() const
{
    return m_damageCursor;
}

quint64 ClientBuffer::damageHandle() const
{
    return m_damageHandle;
}

} // namespace KWaylandServer
