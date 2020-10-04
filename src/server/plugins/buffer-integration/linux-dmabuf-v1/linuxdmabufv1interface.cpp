/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "linuxdmabufv1interface_p.h"
#include "clientbuffermanager_p.h"
#include "display.h"
#include "display_p.h"
#include "drm_fourcc.h"
#include "linuxdmabufv1clientbufferintegration_p.h"

#include <unistd.h>

namespace KWaylandServer
{

static const quint32 s_version = 3;

LinuxDmaBufV1::LinuxDmaBufV1(LinuxDmaBufV1ClientBufferIntegration *integration)
    : QtWaylandServer::zwp_linux_dmabuf_v1(*integration->display(), s_version)
    , m_integration(integration)
{
}

void LinuxDmaBufV1::zwp_linux_dmabuf_v1_bind_resource(Resource *resource)
{
    const QHash<uint32_t, QVector<uint64_t>> modifiers = m_integration->supportedModifiers();
    for (auto it = modifiers.begin(); it != modifiers.end(); ++it) {
        const uint32_t format = it.key();
        QVector<uint64_t> modifiers = it.value();
        if (modifiers.isEmpty()) {
            modifiers.append(DRM_FORMAT_MOD_INVALID);
        }

        for (const uint64_t &modifier : qAsConst(modifiers)) {
            if (resource->version() >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
                const uint32_t modifier_lo = modifier & 0xffffffff;
                const uint32_t modifier_hi = modifier >> 32;
                send_modifier(resource->handle, format, modifier_hi, modifier_lo);
            } else if (modifier == DRM_FORMAT_MOD_LINEAR || modifier == DRM_FORMAT_MOD_INVALID) {
                send_format(resource->handle, format);
            }
        }
    }
}

void LinuxDmaBufV1::zwp_linux_dmabuf_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LinuxDmaBufV1::zwp_linux_dmabuf_v1_create_params(Resource *resource, uint32_t params_id)
{
    wl_resource *paramsResource = wl_resource_create(resource->client(),
                                                     &zwp_linux_buffer_params_v1_interface,
                                                     resource->version(), params_id);
    if (!paramsResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    new LinuxDmaBufParamsV1(m_integration, paramsResource);
}

LinuxDmaBufParamsV1::LinuxDmaBufParamsV1(LinuxDmaBufV1ClientBufferIntegration *integration, ::wl_resource *resource)
    : QtWaylandServer::zwp_linux_buffer_params_v1(resource)
    , m_integration(integration)
    , m_planes(4)
{
}

LinuxDmaBufParamsV1::~LinuxDmaBufParamsV1()
{
    for (const LinuxDmaBufV1Plane &plane : m_planes) {
        if (plane.fileDescriptor != -1) {
            close(plane.fileDescriptor);
        }
    }
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_add(Resource *resource, int32_t fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint32_t modifier_hi, uint32_t modifier_lo)
{
    if (Q_UNLIKELY(m_isUsed)) {
        wl_resource_post_error(resource->handle, error_already_used,
                               "the params object has already been used to create a wl_buffer");
        close(fd);
        return;
    }

    if (Q_UNLIKELY(plane_idx >= uint(m_planes.size()))) {
        wl_resource_post_error(resource->handle, error_plane_idx,
                               "plane index %d is out of bounds", plane_idx);
        close(fd);
        return;
    }

    LinuxDmaBufV1Plane &plane = m_planes[plane_idx];

    if (Q_UNLIKELY(plane.fileDescriptor != -1)) {
        wl_resource_post_error(resource->handle, error_plane_set,
                               "the plane index %d was already set", plane_idx);
        close(fd);
        return;
    }

    plane.fileDescriptor = fd;
    plane.modifier = (quint64(modifier_hi) << 32) | modifier_lo;
    plane.offset = offset;
    plane.stride = stride;

    m_planeCount++;
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_create(Resource *resource, int32_t width, int32_t height, uint32_t format, uint32_t flags)
{
    if (Q_UNLIKELY(m_isUsed)) {
        wl_resource_post_error(resource->handle, error_already_used,
                               "the params object has already been used to create a wl_buffer");
        return;
    }

    if (Q_UNLIKELY(!test(resource, width, height))) {
        return;
    }

    m_isUsed = true;

    wl_resource *bufferResource = wl_resource_create(resource->client(), &wl_buffer_interface, 1, 0);
    if (!bufferResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    m_planes.resize(m_planeCount);

    LinuxDmaBufV1ClientBuffer *buffer = new LinuxDmaBufV1ClientBuffer(m_integration, bufferResource,
                                                                      width, height, format, flags,
                                                                      m_planes);

    DisplayPrivate *displayPrivate = DisplayPrivate::get(m_integration->display());
    displayPrivate->bufferManager->registerClientBuffer(buffer);

    if (buffer->import()) {
        send_created(resource->handle, bufferResource);
    } else {
        send_failed(resource->handle);
    }

    m_planes.clear(); // the ownership of file descriptors has been moved to the buffer
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_create_immed(Resource *resource, uint32_t buffer_id, int32_t width, int32_t height, uint32_t format, uint32_t flags)
{
    if (Q_UNLIKELY(m_isUsed)) {
        wl_resource_post_error(resource->handle, error_already_used,
                               "the params object has already been used to create a wl_buffer");
        return;
    }

    if (Q_UNLIKELY(!test(resource, width, height))) {
        return;
    }

    m_isUsed = true;

    wl_resource *bufferResource = wl_resource_create(resource->client(), &wl_buffer_interface, 1, buffer_id);
    if (!bufferResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    m_planes.resize(m_planeCount);

    LinuxDmaBufV1ClientBuffer *buffer = new LinuxDmaBufV1ClientBuffer(m_integration, bufferResource,
                                                                      width, height, format, flags,
                                                                      m_planes);

    DisplayPrivate *displayPrivate = DisplayPrivate::get(m_integration->display());
    displayPrivate->bufferManager->registerClientBuffer(buffer);

    if (!buffer->import()) {
        wl_resource_post_error(resource->handle, error_invalid_wl_buffer,
                               "importing the supplied dmabufs failed");
    }

    m_planes.clear(); // the ownership of file descriptors has been moved to the buffer
}

bool LinuxDmaBufParamsV1::test(Resource *resource, uint32_t width, uint32_t height)
{
    if (Q_UNLIKELY(!m_planeCount)) {
        wl_resource_post_error(resource->handle, error_incomplete, "no planes have been specified");
        return false;
    }

    // Check for holes in the dmabuf set (e.g. [0, 1, 3]).
    for (int i = 0; i < m_planeCount; ++i) {
        if (m_planes[i].fileDescriptor == -1) {
            wl_resource_post_error(resource->handle, error_incomplete,
                                   "no dmabuf has been added for plane %d", i);
            return false;
        }
    }

    if (Q_UNLIKELY(width == 0 || height == 0)) {
        wl_resource_post_error(resource->handle, error_invalid_dimensions,
                               "invalid width %d or height %d", width, height);
        return false;
    }

    for (int i = 0; i < m_planeCount; ++i) {
        const LinuxDmaBufV1Plane &plane = m_planes.at(i);

        // Check for overflows.
        if (Q_UNLIKELY(uint64_t(plane.offset) + plane.stride > UINT32_MAX)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds,
                                   "size overflow for plane %d", i);
            return false;
        }

        if (Q_UNLIKELY(i == 0 &&
                uint64_t(plane.offset) + uint64_t(plane.stride) * height > UINT32_MAX)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds,
                                   "size overflow for plane %d", i);
            return false;
        }

         // Don't report an error as it might be caused by the kernel not supporting
        // seeking on dmabuf.
        const off_t size = lseek(plane.fileDescriptor, 0, SEEK_END);
        if (size == -1) {
            continue;
        }

        if (Q_UNLIKELY(plane.offset >= size)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds,
                                   "invalid offset %i for plane %d", plane.offset, i);
            return false;
        }

        if (Q_UNLIKELY(plane.offset + plane.stride > size)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds,
                                   "invalid stride %i for plane %d", plane.stride, i);
            return false;
        }

        // Only valid for first plane as other planes might be sub-sampled according to
        // fourcc format.
        if (Q_UNLIKELY(i == 0 && plane.offset + plane.stride * height > size)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds,
                                   "invalid buffer stride of height for plane %d", i);
            return false;
        }
    }

    return true;
}

} // namespace KWaylandServer
