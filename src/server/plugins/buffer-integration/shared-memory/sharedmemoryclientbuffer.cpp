/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "sharedmemoryclientbuffer_p.h"
#include "display.h"
#include "logging.h"
#include "rendererinterface.h"
#include "sharedmemoryclientbufferintegration_p.h"
#include "utils.h"

#include <epoxy/egl.h>
#include <wayland-server.h>

namespace KWaylandServer
{

// TODO: Finish the support for multi-plane client buffer formats.

static SharedMemoryClientBuffer *s_accessedBuffer = nullptr;
static int s_accessCounter = 0;

static bool alphaChannelFromFormat(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
        return true;
    case WL_SHM_FORMAT_XRGB8888:
    default:
        return false;
    }
}

SharedMemoryClientBuffer::SharedMemoryClientBuffer(::wl_resource *resource,
                                                   SharedMemoryClientBufferIntegration *integration)
    : ClientBuffer(integration, resource)
    , m_integration(integration)
{
    wl_shm_buffer *buffer = wl_shm_buffer_get(resource);
    m_width = wl_shm_buffer_get_width(buffer);
    m_height = wl_shm_buffer_get_height(buffer);
    m_hasAlphaChannel = alphaChannelFromFormat(wl_shm_buffer_get_format(buffer));

    auto rendererPrivate = RendererInterfacePrivate::get(integration->display()->rendererInterface());
    rendererPrivate->addEventListener(this);
}

SharedMemoryClientBuffer::~SharedMemoryClientBuffer()
{
    cleanup();

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->removeEventListener(this);
}

ClientBufferRef::Origin SharedMemoryClientBuffer::origin() const
{
    return ClientBufferRef::Origin::TopLeft;
}

ClientBufferRef::TextureType SharedMemoryClientBuffer::textureType() const
{
    return ClientBufferRef::TextureType::RGBA;
}

QSize SharedMemoryClientBuffer::size() const
{
    return QSize(m_width, m_height);
}

bool SharedMemoryClientBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

static QImage::Format imageFormatForShmFormat(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32_Premultiplied;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

static void cleanupSharedMemoryImage(void *bufferHandle)
{
    Q_ASSERT_X(s_accessCounter > 0, "cleanup", "access counter must be positive");
    s_accessCounter--;
    if (s_accessCounter == 0) {
        s_accessedBuffer = nullptr;
    }
    wl_shm_buffer_end_access(static_cast<wl_shm_buffer *>(bufferHandle));
}

QImage SharedMemoryClientBuffer::toImage()
{
    if (s_accessedBuffer && s_accessedBuffer != this) {
        return QImage();
    }

    if (wl_shm_buffer *buffer = wl_shm_buffer_get(handle())) {
        s_accessedBuffer = this;
        s_accessCounter++;
        wl_shm_buffer_begin_access(buffer);
        const uchar *data = static_cast<const uchar *>(wl_shm_buffer_get_data(buffer));
        const uint32_t stride = wl_shm_buffer_get_stride(buffer);
        const uint32_t format = wl_shm_buffer_get_format(buffer);
        return QImage(data, m_width, m_height, stride, imageFormatForShmFormat(format),
                      cleanupSharedMemoryImage, buffer);
    }
    return QImage();
}

static void allocateStorage(RendererInterface *rendererInterface, int width, int height)
{
    GLint internalFormat;
    GLenum externalFormat;

    if (rendererInterface->graphicsApi() == RendererInterface::GraphicsApi::OpenGLES) {
        if (rendererInterface->supportsARGB32()) {
            internalFormat = GL_BGRA_EXT;
            externalFormat = GL_BGRA_EXT;
        } else {
            internalFormat = GL_RGBA;
            externalFormat = GL_RGBA;
        }
    } else {
        internalFormat = GL_RGBA8;
        externalFormat = GL_BGRA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                 externalFormat, GL_UNSIGNED_BYTE, nullptr);
}

static void updateTexture(RendererInterface *rendererInterface, const QImage &image, const QRegion &damage)
{
    GLenum externalFormat;
    QImage converted;

    if (rendererInterface->graphicsApi() == RendererInterface::GraphicsApi::OpenGLES) {
        if (rendererInterface->supportsARGB32()) {
            converted = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            externalFormat = GL_BGRA_EXT;
        } else {
            converted = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            externalFormat = GL_RGBA;
        }
    } else {
        converted = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        externalFormat = GL_BGRA;
    }

    for (const QRect &rect : damage) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                        externalFormat, GL_UNSIGNED_BYTE, converted.copy(rect).constBits());
    }
}

GLuint SharedMemoryClientBuffer::toOpenGLTexture(int plane)
{
    Q_ASSERT_X(eglGetCurrentContext(), "toOpenGLTexture()", "no current context");

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->destroyZombieResources();

    if (plane != 0) {
        qCWarning(KWS_SHM) << "Multi-planar shared memory buffers are unsupported";
        return 0;
    }

    GLuint &texture = m_textures[plane];

    if (!texture) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        allocateStorage(m_integration->display()->rendererInterface(), m_width, m_height);
        m_damageRegion = infiniteRegion();
    }
    if (!m_damageRegion.isEmpty()) {
        const QImage data = toImage();
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        const QRegion dirtyRegion = m_damageRegion & QRect(0, 0, m_width, m_height);
        updateTexture(m_integration->display()->rendererInterface(), data, dirtyRegion);
        m_damageRegion = QRegion();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return texture;
}

void SharedMemoryClientBuffer::handleGraphicsReset()
{
    cleanup();
}

void SharedMemoryClientBuffer::cleanup()
{
    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());

    for (int i = 0; i < m_planeCount; ++i) {
        if (m_textures[i]) {
            rendererPrivate->scheduleRemoveTexture(m_textures[i]);
        }
    }

    m_textures.fill(0);
}

} // namespace KWaylandServer
