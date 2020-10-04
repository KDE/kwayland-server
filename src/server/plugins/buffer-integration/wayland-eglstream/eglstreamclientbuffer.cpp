/*
    SPDX-FileCopyrightText: 2019 NVIDIA Inc.
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglstreamclientbuffer_p.h"
#include "display.h"
#include "eglstreamclientbufferintegration_p.h"
#include "logging.h"
#include "rendererinterface_p.h"
#include "utils.h"

namespace KWaylandServer
{

// TODO: Finish the support for multi-plane client buffer formats.

EglStreamClientBuffer::EglStreamClientBuffer(EglStreamClientBufferIntegration *integration,
                                             ::wl_resource *resource, ::wl_array *attribs)
    : ClientBuffer(integration, resource)
    , m_integration(integration)
{
    EglStreamFunctions *functions = integration->functions();

    // The buffer format is not queried because the upstream doesn't provide any means to do so.

    functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_WIDTH, &m_width);
    functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_HEIGHT, &m_height);

    bool ok = functions->eglQueryWaylandBufferWL(integration->eglDisplay(), resource, EGL_WAYLAND_Y_INVERTED_WL, &m_isYInverted);
    if (!ok) {
        // If EGL_WAYLAND_Y_INVERTED_WL is unsupported, we must assume that the buffer is inverted.
        m_isYInverted = true;
    }

    wl_array_init(&m_attribs);
    wl_array_copy(&m_attribs, attribs);

    auto rendererPrivate = RendererInterfacePrivate::get(integration->display()->rendererInterface());
    rendererPrivate->addEventListener(this);

    attachStream();
}

EglStreamClientBuffer::~EglStreamClientBuffer()
{
    detachStream();

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->removeEventListener(this);

    wl_array_release(&m_attribs);
}

void EglStreamClientBuffer::handleGraphicsReset()
{
    // If the consumer endpoint is detached, the stream will transition in the "disconnected"
    // state. At which point, the only way to proceed is to create another stream. However, it
    // seems like clients do not handle such case, so yes, this will break them.

    detachStream();
}

ClientBufferRef::Origin EglStreamClientBuffer::origin() const
{
    return m_isYInverted ? ClientBufferRef::Origin::TopLeft : ClientBufferRef::Origin::BottomLeft;
}

ClientBufferRef::TextureType EglStreamClientBuffer::textureType() const
{
    return ClientBufferRef::TextureType::RGBA;
}

QSize EglStreamClientBuffer::size() const
{
    return QSize(m_width, m_height);
}

bool EglStreamClientBuffer::hasAlphaChannel() const
{
    return true; // Currently, there is no any way to query the buffer format.
}

GLuint EglStreamClientBuffer::toOpenGLTexture(int plane)
{
    Q_ASSERT_X(eglGetCurrentContext(), "toOpenGLTexture()", "no current context");

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->destroyZombieResources();

    if (Q_UNLIKELY(m_eglStream == EGL_NO_STREAM_KHR)) {
        return 0;
    }

    if (Q_UNLIKELY(plane < 0 || plane >= m_planeCount)) {
        qCWarning(KWS_EGLSTREAM) << "Invalid plane index:" << plane;
        return 0;
    }

    GLuint &texture = m_textures[plane];

    if (!texture) {
        glGenTextures(1, &texture);
        m_damageRegion = infiniteRegion();
    }

    if (!m_damageRegion.isEmpty()) {
        if (acquireStreamFrame()) {
            copyExternalTexture(plane);
            releaseStreamFrame();
        }
        m_damageRegion = QRegion();
    }

    return texture;
}

bool EglStreamClientBuffer::acquireStreamFrame()
{
    const EglStreamFunctions *functions = m_integration->functions();

    EGLAttrib state;
    if (!functions->eglQueryStreamAttribNV(m_integration->eglDisplay(), m_eglStream,
                                           EGL_STREAM_STATE_KHR, &state)) {
        qCWarning(KWS_EGLSTREAM, "Failed to query the state of stream %p (error code 0x%x)",
                  m_eglStream, eglGetError());
        return false;
    }

    if (state != EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR) {
        return false;
    }

    if (!functions->eglStreamConsumerAcquireAttribNV(m_integration->eglDisplay(),
                                                     m_eglStream, nullptr)) {
        qCWarning(KWS_EGLSTREAM, "Failed to acquire the current frame of %p (error code 0x%x)",
                  m_eglStream, eglGetError());
        return false;
    }

    return true;
}

void EglStreamClientBuffer::releaseStreamFrame()
{
    const EglStreamFunctions *functions = m_integration->functions();
    functions->eglStreamConsumerReleaseAttribNV(m_integration->eglDisplay(), m_eglStream, nullptr);
}

bool EglStreamClientBuffer::attachStream()
{
    const EglStreamFunctions *functions = m_integration->functions();

    QVector<EGLAttrib> attribs;
    attribs << EGL_WAYLAND_EGLSTREAM_WL;
    attribs << reinterpret_cast<EGLAttrib>(handle());

    for (uint i = 0; i < m_attribs.size; ++i) {
        attribs << static_cast<EGLAttrib *>(m_attribs.data)[i];
    }

    attribs << EGL_NONE;

    m_eglStream = functions->eglCreateStreamAttribNV(m_integration->eglDisplay(), attribs.constData());
    if (m_eglStream == EGL_NO_STREAM_KHR) {
        qCWarning(KWS_EGLSTREAM, "Failed to create an EGLStream for buffer 0x%x (error 0x%x)",
                  wl_resource_get_id(handle()), eglGetError());
        return false;
    }

    glGenTextures(1, &m_externalTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_externalTexture);
    bool ok = eglStreamConsumerGLTextureExternalKHR(m_integration->eglDisplay(), m_eglStream);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    if (!ok) {
        qCWarning(KWS_EGLSTREAM, "Failed to attach consumer endpoint for buffer 0%x (error 0x%x)",
                  wl_resource_get_id(handle()), eglGetError());
        return false;
    }

    m_planeCount = 1;

    for (int i = 0; i < m_planeCount; ++i) {
        glGenFramebuffers(1, &m_copyFramebuffers[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_copyFramebuffers[i]);
        glGenRenderbuffers(1, &m_copyRenderbuffers[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, m_copyRenderbuffers[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, hasAlphaChannel() ? GL_RGBA : GL_RGB, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_copyRenderbuffers[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    return true;
}

void EglStreamClientBuffer::detachStream()
{
    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());

    for (int i = 0; i < m_planeCount; ++i) {
        if (m_copyFramebuffers[i]) {
            rendererPrivate->scheduleRemoveFramebuffer(m_copyFramebuffers[i]);
        }
        if (m_copyRenderbuffers[i]) {
            rendererPrivate->scheduleRemoveRenderbuffer(m_copyRenderbuffers[i]);
        }
        if (m_textures[i]) {
            rendererPrivate->scheduleRemoveTexture(m_textures[i]);
        }
    }

    if (m_externalTexture) {
        rendererPrivate->scheduleRemoveTexture(m_externalTexture);
    }

    if (m_eglStream != EGL_NO_STREAM_KHR) {
        eglDestroyStreamKHR(m_integration->eglDisplay(), m_eglStream);
    }

    m_planeCount = 0;
    m_copyFramebuffers.fill(0);
    m_copyRenderbuffers.fill(0);
    m_textures.fill(0);

    m_externalTexture = 0;
    m_eglStream = EGL_NO_STREAM_KHR;
}

void EglStreamClientBuffer::copyExternalTexture(int plane)
{
    const GLuint format = hasAlphaChannel() ? GL_RGBA : GL_RGB;

    GLint oldViewport[4], oldProgram;
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glViewport(0, 0, m_width, m_height);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_copyFramebuffers[plane]);
    glBindRenderbuffer(GL_RENDERBUFFER, m_copyRenderbuffers[plane]);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_externalTexture);
    glEnable(GL_TEXTURE_EXTERNAL_OES);

    GLfloat yTop = m_isYInverted ? 0 : 1;
    glBegin(GL_QUADS);
    glTexCoord2f(0, yTop);
    glVertex2f(-1, 1);
    glTexCoord2f(0, 1 - yTop);
    glVertex2f(-1, -1);
    glTexCoord2f(1, 1 - yTop);
    glVertex2f(1, -1);
    glTexCoord2f(1, yTop);
    glVertex2f(1, 1);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, m_textures[plane]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, format, 0, 0, m_width, m_height, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(oldProgram);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

} // namespace KWaylandServer
