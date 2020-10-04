/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer_p.h"
#include "rendererinterface_p.h"

namespace KWaylandServer
{

class SharedMemoryClientBufferIntegration;

class SharedMemoryClientBuffer : public ClientBuffer, public RendererEventListener
{
    Q_OBJECT

public:
    SharedMemoryClientBuffer(::wl_resource *resource, SharedMemoryClientBufferIntegration *integration);
    ~SharedMemoryClientBuffer() override;

    ClientBufferRef::Origin origin() const override;
    ClientBufferRef::TextureType textureType() const override;
    QSize size() const override;
    bool hasAlphaChannel() const override;
    GLuint toOpenGLTexture(int plane) override;
    QImage toImage() override;

protected:
    void handleGraphicsReset() override;

private:
    void cleanup();

    SharedMemoryClientBufferIntegration *m_integration;
    std::array<GLuint, 4> m_textures = {};
    int m_planeCount = 1;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_hasAlphaChannel = false;
};

} // namespace KWaylandServer
