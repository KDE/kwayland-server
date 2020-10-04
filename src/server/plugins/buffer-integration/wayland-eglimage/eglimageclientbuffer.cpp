/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "eglimageclientbuffer_p.h"
#include "display.h"
#include "eglimageclientbufferintegration_p.h"
#include "logging.h"
#include "utils.h"

#include <wayland-server-core.h>

namespace KWaylandServer
{

// TODO: Finish the support for multi-plane client buffer formats.

EglImageClientBuffer::EglImageClientBuffer(EglImageClientBufferIntegration *integration, ::wl_resource *resource)
    : ClientBuffer(integration, resource)
    , m_integration(integration)
{
    EglImageFunctions *functions = integration->functions();

    functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_TEXTURE_FORMAT, &m_format);
    functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_WIDTH, &m_width);
    functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_HEIGHT, &m_height);

    bool ok = functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_WAYLAND_Y_INVERTED_WL, &m_isYInverted);
    if (!ok) {
        // If EGL_WAYLAND_Y_INVERTED_WL is unsupported, we must assume that the buffer is inverted.
        m_isYInverted = true;
    }

    auto rendererPrivate = RendererInterfacePrivate::get(integration->display()->rendererInterface());
    rendererPrivate->addEventListener(this);
}

EglImageClientBuffer::~EglImageClientBuffer()
{
    cleanup();

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->removeEventListener(this);
}

void EglImageClientBuffer::handleGraphicsReset()
{
    cleanup();
}

ClientBufferRef::Origin EglImageClientBuffer::origin() const
{
    return m_isYInverted ? ClientBufferRef::Origin::TopLeft : ClientBufferRef::Origin::BottomLeft;
}

ClientBufferRef::TextureType EglImageClientBuffer::textureType() const
{
    return ClientBufferRef::TextureType::RGBA;
}

QSize EglImageClientBuffer::size() const
{
    return QSize(m_width, m_height);
}

bool EglImageClientBuffer::hasAlphaChannel() const
{
    return m_format == EGL_TEXTURE_RGBA;
}

GLuint EglImageClientBuffer::toOpenGLTexture(int plane)
{
    Q_ASSERT_X(eglGetCurrentContext(), "toOpenGLTexture()", "no current context");

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->destroyZombieResources();

    if (Q_UNLIKELY(!m_isImported)) {
        import();
    }

    if (Q_UNLIKELY(plane < 0 || plane >= m_planeCount)) {
        qCWarning(KWS_EGLIMAGE, "Invalid plane index %i, plane count %i", plane, m_planeCount);
        return 0;
    }

    if (Q_UNLIKELY(m_images[plane] == EGL_NO_IMAGE_KHR)) {
        return 0;
    }

    GLuint &texture = m_textures[plane];

    if (!texture) {
        glGenTextures(1, &texture);
        m_damageRegion = infiniteRegion();
    }

    if (!m_damageRegion.isEmpty()) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_images[plane]);
        m_damageRegion = QRegion();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return texture;
}

void EglImageClientBuffer::import()
{
    Q_ASSERT_X(!m_isImported, "import", "Importing already imported client buffer");

    switch (m_format) {
    case EGL_TEXTURE_RGB:
    case EGL_TEXTURE_RGBA:
        m_planeCount = 1;
        break;
    case EGL_TEXTURE_EXTERNAL_WL:
        qCWarning(KWS_EGLIMAGE, "EGL_TEXTURE_EXTERNAL_WL texture format is unsupported");
        return;
    case EGL_TEXTURE_Y_UV_WL:
        qCWarning(KWS_EGLIMAGE, "EGL_TEXTURE_Y_UV_WL texture format is unsupported");
        return;
    case EGL_TEXTURE_Y_U_V_WL:
        qCWarning(KWS_EGLIMAGE, "EGL_TEXTURE_Y_U_V_WL texture format is unsupported");
        return;
    case EGL_TEXTURE_Y_XUXV_WL:
        qCWarning(KWS_EGLIMAGE, "EGL_TEXTURE_Y_XUXV_WL texture format is unsupported");
        return;
    default:
        qCWarning(KWS_EGLIMAGE, "Unknown texture format: 0x%x", m_format);
        return;
    }

    for (int i = 0; i < m_planeCount; ++i) {
        const EGLint attribs[] = { EGL_WAYLAND_PLANE_WL, i, EGL_NONE };

        EGLImageKHR image = eglCreateImageKHR(m_integration->eglDisplay(), EGL_NO_CONTEXT,
                                              EGL_WAYLAND_BUFFER_WL, handle(), attribs);

        if (image == EGL_NO_IMAGE_KHR) {
            qCWarning(KWS_EGLIMAGE, "Failed to import plane %i for buffer %i (error code 0x%x)",
                      i, wl_resource_get_id(handle()), eglGetError());
            continue;
        }

        m_images[i] = image;
    }

    m_isImported = true;
}

void EglImageClientBuffer::cleanup()
{
    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());

    for (int i = 0; i < m_planeCount; ++i) {
        if (m_textures[i]) {
            rendererPrivate->scheduleRemoveTexture(m_textures[i]);
        }
        if (m_images[i] != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(m_integration->eglDisplay(), m_images[i]);
        }
    }

    m_planeCount = 0;
    m_textures.fill(0);
    m_images.fill(EGL_NO_IMAGE_KHR);
    m_isImported = false;
}

} // namespace KWaylandServer
