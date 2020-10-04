/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clientbuffer_p.h"
#include "rendererinterface_p.h"

namespace KWaylandServer
{

class EglStreamClientBufferIntegration;

class EglStreamClientBuffer : public ClientBuffer, public RendererEventListener
{
    Q_OBJECT

public:
    EglStreamClientBuffer(EglStreamClientBufferIntegration *integration,
                          ::wl_resource *resource, ::wl_array *attribs);
    ~EglStreamClientBuffer() override;

    ClientBufferRef::Origin origin() const override;
    ClientBufferRef::TextureType textureType() const override;
    QSize size() const override;
    GLuint toOpenGLTexture(int plane) override;
    bool hasAlphaChannel() const override;

protected:
    void handleGraphicsReset() override;

private:
    bool acquireStreamFrame();
    void releaseStreamFrame();
    bool attachStream();
    void detachStream();
    void copyExternalTexture(int plane);

    EglStreamClientBufferIntegration *m_integration;
    wl_array m_attribs;
    EGLStreamKHR m_eglStream = EGL_NO_STREAM_KHR;
    std::array<GLuint, 4> m_textures = {};
    GLuint m_externalTexture = 0;
    std::array<GLuint, 4> m_copyRenderbuffers = {};
    std::array<GLuint, 4> m_copyFramebuffers = {};
    int m_planeCount = 0;
    int m_width = 0;
    int m_height = 0;
    int m_isYInverted = 0;
};

} // namespace KWaylandServer
