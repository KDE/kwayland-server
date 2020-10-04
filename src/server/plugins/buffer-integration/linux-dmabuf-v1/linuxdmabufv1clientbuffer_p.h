/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer_p.h"
#include "rendererinterface_p.h"

#include "qwayland-server-wayland.h"

#include <epoxy/egl.h>

namespace KWaylandServer
{

class LinuxDmaBufV1ClientBufferIntegration;

struct LinuxDmaBufV1Plane
{
    int fileDescriptor = -1;
    quint32 offset = 0;
    quint32 stride = 0;
    quint64 modifier = 0;
};

class LinuxDmaBufV1ClientBuffer : public ClientBuffer, public RendererEventListener, public QtWaylandServer::wl_buffer
{
    Q_OBJECT

public:
    LinuxDmaBufV1ClientBuffer(LinuxDmaBufV1ClientBufferIntegration *integration,
                              ::wl_resource *resource, uint32_t width, uint32_t height,
                              uint32_t format, uint32_t flags,
                              const QVector<LinuxDmaBufV1Plane> &planes);
    ~LinuxDmaBufV1ClientBuffer() override;

    bool import();

    ClientBufferRef::Origin origin() const override;
    ClientBufferRef::TextureType textureType() const override;
    GLuint toOpenGLTexture(int plane) override;
    QSize size() const override;
    bool hasAlphaChannel() const override;

protected:
    void handleGraphicsReset() override;
    void buffer_destroy(Resource *resource) override;

private:
    void cleanup();

    bool tryImportSimpleDmaBuf();
    bool tryImportYuvDmaBuf();

    LinuxDmaBufV1ClientBufferIntegration *m_integration;
    QVector<GLuint> m_textures;
    QVector<EGLImageKHR> m_images;
    QVector<LinuxDmaBufV1Plane> m_planes;
    QSize m_size;
    uint32_t m_format;
    uint32_t m_flags;
    bool m_hasAlphaChannel = false;
};

} // namespace KWaylandServer
