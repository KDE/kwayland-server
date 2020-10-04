/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "linuxdmabufv1clientbuffer_p.h"

#include "qwayland-server-linux-dmabuf-unstable-v1.h"

namespace KWaylandServer
{

class LinuxDmaBufV1ClientBufferIntegration;

class LinuxDmaBufV1 : public QtWaylandServer::zwp_linux_dmabuf_v1
{
public:
    explicit LinuxDmaBufV1(LinuxDmaBufV1ClientBufferIntegration *integration);

protected:
    void zwp_linux_dmabuf_v1_bind_resource(Resource *resource) override;
    void zwp_linux_dmabuf_v1_destroy(Resource *resource) override;
    void zwp_linux_dmabuf_v1_create_params(Resource *resource, uint32_t params_id) override;

private:
    LinuxDmaBufV1ClientBufferIntegration *m_integration;
};

class LinuxDmaBufParamsV1 : public QtWaylandServer::zwp_linux_buffer_params_v1
{
public:
    LinuxDmaBufParamsV1(LinuxDmaBufV1ClientBufferIntegration *integration, ::wl_resource *resource);
    ~LinuxDmaBufParamsV1() override;

protected:
    void zwp_linux_buffer_params_v1_destroy_resource(Resource *resource) override;
    void zwp_linux_buffer_params_v1_destroy(Resource *resource) override;
    void zwp_linux_buffer_params_v1_add(Resource *resource, int32_t fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint32_t modifier_hi, uint32_t modifier_lo) override;
    void zwp_linux_buffer_params_v1_create(Resource *resource, int32_t width, int32_t height, uint32_t format, uint32_t flags) override;
    void zwp_linux_buffer_params_v1_create_immed(Resource *resource, uint32_t buffer_id, int32_t width, int32_t height, uint32_t format, uint32_t flags) override;

private:
    bool test(Resource *resource, uint32_t width, uint32_t height);

    LinuxDmaBufV1ClientBufferIntegration *m_integration;
    QVector<LinuxDmaBufV1Plane> m_planes;
    int m_planeCount = 0;
    bool m_isUsed = false;
};

} // namespace KWaylandServer
