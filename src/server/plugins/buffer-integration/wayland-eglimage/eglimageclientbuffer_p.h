/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer_p.h"
#include "rendererinterface_p.h"

namespace KWaylandServer
{

class EglImageClientBufferIntegration;

class EglImageClientBuffer : public ClientBuffer, public RendererEventListener
{
    Q_OBJECT

public:
    EglImageClientBuffer(EglImageClientBufferIntegration *integration, ::wl_resource *resource);
    ~EglImageClientBuffer() override;

    void import();

    ClientBufferRef::Origin origin() const override;
    ClientBufferRef::TextureType textureType() const override;
    QSize size() const override;
    GLuint toOpenGLTexture(int plane) override;
    bool hasAlphaChannel() const override;

protected:
    void handleGraphicsReset() override;

private:
    void cleanup();

    EglImageClientBufferIntegration *m_integration;
    std::array<EGLImageKHR, 4> m_images = {};
    std::array<GLuint, 4> m_textures = {};
    int m_planeCount = 0;
    int m_format = 0;
    int m_width = 0;
    int m_height = 0;
    int m_isYInverted = 0;
    bool m_isImported = false;
};

} // namespace KWaylandServer
