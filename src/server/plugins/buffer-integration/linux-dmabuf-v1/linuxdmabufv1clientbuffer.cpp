/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "linuxdmabufv1clientbuffer_p.h"
#include "display.h"
#include "drm_fourcc.h"
#include "linuxdmabufv1clientbufferintegration_p.h"
#include "logging.h"
#include "utils.h"

#include "qwayland-server-linux-dmabuf-unstable-v1.h"

#include <unistd.h>

namespace KWaylandServer
{

struct YuvPlaneDescriptor
{
    int widthDivisor;
    int heightDivisor;
    uint32_t format;
    int planeIndex;
};

struct YuvFormatDescriptor
{
    bool isValid() const { return format; }

    uint32_t format;
    int inputPlaneCount;
    int outputPlaneCount;
    int textureType;
    YuvPlaneDescriptor planes[4];
};

static YuvFormatDescriptor yuvFormatForDrmFormat(uint32_t drmFormat)
{
    static YuvFormatDescriptor descriptors[] = {
        {
            .format = DRM_FORMAT_YUYV,
            .inputPlaneCount = 1,
            .outputPlaneCount = 2,
            .textureType = EGL_TEXTURE_Y_XUXV_WL,
            .planes = {
                {
                    .widthDivisor = 1,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_GR88,
                    .planeIndex = 0
                },
                {
                    .widthDivisor = 2,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_ARGB8888,
                    .planeIndex = 0
                }
            }
        },
        {
            .format = DRM_FORMAT_NV12,
            .inputPlaneCount = 2,
            .outputPlaneCount = 2,
            .textureType = EGL_TEXTURE_Y_UV_WL,
            .planes = {
                {
                    .widthDivisor = 1,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 0
                },
                {
                    .widthDivisor = 2,
                    .heightDivisor = 2,
                    .format = DRM_FORMAT_GR88,
                    .planeIndex = 1
                }
            }
        },
        {
            .format = DRM_FORMAT_YUV420,
            .inputPlaneCount = 3,
            .outputPlaneCount = 3,
            .textureType = EGL_TEXTURE_Y_U_V_WL,
            .planes = {
                {
                    .widthDivisor = 1,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 0
                },
                {
                    .widthDivisor = 2,
                    .heightDivisor = 2,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 1
                },
                {
                    .widthDivisor = 2,
                    .heightDivisor = 2,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 2
                }
            }
        },
        {
            .format = DRM_FORMAT_YUV444,
            .inputPlaneCount = 3,
            .outputPlaneCount = 3,
            .textureType = EGL_TEXTURE_Y_U_V_WL,
            .planes = {
                {
                    .widthDivisor = 1,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 0
                },
                {
                    .widthDivisor = 1,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 1
                },
                {
                    .widthDivisor = 1,
                    .heightDivisor = 1,
                    .format = DRM_FORMAT_R8,
                    .planeIndex = 2
                }
            }
        }
    };

    for (const YuvFormatDescriptor &descriptor : descriptors) {
        if (descriptor.format == drmFormat) {
            return descriptor;
        }
    }

    return YuvFormatDescriptor{};
}

static bool testAlphaChannel(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:

    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:

    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:

    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:

    case DRM_FORMAT_XRGB8888_A8:
    case DRM_FORMAT_XBGR8888_A8:
    case DRM_FORMAT_RGBX8888_A8:
    case DRM_FORMAT_BGRX8888_A8:
    case DRM_FORMAT_RGB888_A8:
    case DRM_FORMAT_BGR888_A8:
    case DRM_FORMAT_RGB565_A8:
    case DRM_FORMAT_BGR565_A8:
        return true;
    default:
        return false;
    }
}

LinuxDmaBufV1ClientBuffer::LinuxDmaBufV1ClientBuffer(LinuxDmaBufV1ClientBufferIntegration *integration,
                                                     ::wl_resource *resource, uint32_t width,
                                                     uint32_t height, uint32_t format, uint32_t flags,
                                                     const QVector<LinuxDmaBufV1Plane> &planes)
    : ClientBuffer(integration, resource)
    , QtWaylandServer::wl_buffer(resource)
    , m_integration(integration)
    , m_planes(planes)
    , m_size(width, height)
    , m_format(format)
    , m_flags(flags)
    , m_hasAlphaChannel(testAlphaChannel(format))
{
    m_textures.resize(m_planes.count());
    m_images.resize(m_planes.count());

    auto rendererPrivate = RendererInterfacePrivate::get(integration->display()->rendererInterface());
    rendererPrivate->addEventListener(this);
}

LinuxDmaBufV1ClientBuffer::~LinuxDmaBufV1ClientBuffer()
{
    const EGLDisplay eglDisplay = m_integration->eglDisplay();

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->removeEventListener(this);

    for (int i = 0; i < m_planes.count(); ++i) {
        if (m_textures[i]) {
            rendererPrivate->scheduleRemoveTexture(m_textures[i]);
        }
        if (m_images[i] != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(eglDisplay, m_images[i]);
        }
        if (m_planes[i].fileDescriptor != -1) {
            close(m_planes[i].fileDescriptor);
        }
    }
}

static uint32_t low(uint64_t value)
{
    return value & 0xffffffff;
}

static uint32_t high(uint64_t value)
{
    return value >> 32;
}

static EGLImageKHR import_helper(const LinuxDmaBufV1ClientBufferIntegration *integration,
                                 const QVector<LinuxDmaBufV1Plane> &planes, uint32_t format,
                                 const QSize &size)
{
    const LinuxDmaBufV1EglFunctions *functions = integration->functions();

    const bool hasModifiers = functions->eglQueryDmaBufModifiersEXT != nullptr &&
            planes[0].modifier != DRM_FORMAT_MOD_INVALID;

    QVector<EGLint> attributes;
    attributes << EGL_WIDTH;
    attributes << size.width();
    attributes << EGL_HEIGHT;
    attributes << size.height();
    attributes << EGL_LINUX_DRM_FOURCC_EXT;
    attributes << format;

    attributes << EGL_DMA_BUF_PLANE0_FD_EXT;
    attributes << planes[0].fileDescriptor;
    attributes << EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attributes << planes[0].offset;
    attributes << EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attributes << planes[0].stride;
    if (hasModifiers) {
        attributes << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attributes << high(planes[0].modifier);
        attributes << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attributes << low(planes[0].modifier);
    }

    if (planes.count() > 1) {
        attributes << EGL_DMA_BUF_PLANE1_FD_EXT;
        attributes << planes[1].fileDescriptor;
        attributes << EGL_DMA_BUF_PLANE1_OFFSET_EXT;
        attributes << planes[1].offset;
        attributes << EGL_DMA_BUF_PLANE1_PITCH_EXT;
        attributes << planes[1].stride;
        if (hasModifiers) {
            attributes << EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
            attributes << high(planes[1].modifier);
            attributes << EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
            attributes << low(planes[1].modifier);
        }
    }

    if (planes.count() > 2) {
        attributes << EGL_DMA_BUF_PLANE2_FD_EXT;
        attributes << planes[2].fileDescriptor;
        attributes << EGL_DMA_BUF_PLANE2_OFFSET_EXT;
        attributes << planes[2].offset;
        attributes << EGL_DMA_BUF_PLANE2_PITCH_EXT;
        attributes << planes[2].stride;
        if (hasModifiers) {
            attributes << EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
            attributes << high(planes[2].modifier);
            attributes << EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
            attributes << low(planes[2].modifier);
        }
    }

    if (planes.count() > 3) {
        attributes << EGL_DMA_BUF_PLANE3_FD_EXT;
        attributes << planes[3].fileDescriptor;
        attributes << EGL_DMA_BUF_PLANE3_OFFSET_EXT;
        attributes << planes[3].offset;
        attributes << EGL_DMA_BUF_PLANE3_PITCH_EXT;
        attributes << planes[3].stride;
        if (hasModifiers) {
            attributes << EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
            attributes << high(planes[3].modifier);
            attributes << EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
            attributes << low(planes[3].modifier);
        }
    }

    attributes << EGL_NONE;

    return eglCreateImageKHR(integration->eglDisplay(), EGL_NO_CONTEXT,
                             EGL_LINUX_DMA_BUF_EXT, static_cast<EGLClientBuffer>(nullptr),
                             attributes.constData());
}

bool LinuxDmaBufV1ClientBuffer::tryImportSimpleDmaBuf()
{
    EGLImageKHR image = import_helper(m_integration, m_planes, m_format, m_size);
    if (image == EGL_NO_IMAGE_KHR) {
        qCWarning(KWS_LINUXDMABUF, "Failed to import a dmabuf (error code 0x%x)", eglGetError());
        return false;
    }

    m_images[0] = image;
    return true;
}

bool LinuxDmaBufV1ClientBuffer::tryImportYuvDmaBuf()
{
    const YuvFormatDescriptor yuvFormat = yuvFormatForDrmFormat(m_format);
    if (!yuvFormat.isValid()) {
        qCWarning(KWS_LINUXDMABUF) << "Could not find YUV format for DRM format" << m_format;
        return false;
    }

    if (yuvFormat.inputPlaneCount != m_planes.count()) {
        qCWarning(KWS_LINUXDMABUF, "Invalid yuv buffer, expected %d input planes, given %d",
                  yuvFormat.inputPlaneCount, m_planes.count());
        return false;
    }

    for (int i = 0; i < yuvFormat.outputPlaneCount; ++i) {
        const YuvPlaneDescriptor planeDescriptor = yuvFormat.planes[i];
        const QVector<LinuxDmaBufV1Plane> planes { m_planes[planeDescriptor.planeIndex] };

        const QSize planeSize(m_size.width() / planeDescriptor.widthDivisor,
                              m_size.height() / planeDescriptor.heightDivisor);

        EGLImageKHR image = import_helper(m_integration, planes, planeDescriptor.format, planeSize);
        if (image == EGL_NO_IMAGE_KHR) {
            qCWarning(KWS_LINUXDMABUF) << "Failed to import YUV plane" << i;
            for (int j = 0; j < i; ++j) {
                eglDestroyImageKHR(m_integration->eglDisplay(), m_images[j]);
                m_images[j] = EGL_NO_IMAGE_KHR;
            }
            return false;
        }

        m_images[i] = image;
    }

    return true;
}

bool LinuxDmaBufV1ClientBuffer::import()
{
    return tryImportSimpleDmaBuf() || tryImportYuvDmaBuf();
}

GLuint LinuxDmaBufV1ClientBuffer::toOpenGLTexture(int plane)
{
    Q_ASSERT_X(eglGetCurrentContext(), "toOpenGLTexture()", "no current context");

    auto rendererPrivate = RendererInterfacePrivate::get(m_integration->display()->rendererInterface());
    rendererPrivate->destroyZombieResources();

    if (Q_UNLIKELY(m_images[0] == EGL_NO_IMAGE_KHR)) {
        if (!import()) {
            return 0;
        }
    }

    GLuint &texture = m_textures[plane];

    if (Q_UNLIKELY(!texture)) {
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

ClientBufferRef::Origin LinuxDmaBufV1ClientBuffer::origin() const
{
    if (m_flags & QtWaylandServer::zwp_linux_buffer_params_v1::flags_y_invert) {
        return ClientBufferRef::Origin::BottomLeft;
    } else {
        return ClientBufferRef::Origin::TopLeft;
    }
}

ClientBufferRef::TextureType LinuxDmaBufV1ClientBuffer::textureType() const
{
    return ClientBufferRef::TextureType::RGBA;
}

QSize LinuxDmaBufV1ClientBuffer::size() const
{
    return m_size;
}

bool LinuxDmaBufV1ClientBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

void LinuxDmaBufV1ClientBuffer::handleGraphicsReset()
{
    const EGLDisplay eglDisplay = m_integration->eglDisplay();

    for (int i = 0; i < m_planes.count(); ++i) {
        if (m_textures[i]) {
            glDeleteTextures(1, &m_textures[i]);
            m_textures[i] = 0;
        }
        if (m_images[i]) {
            eglDestroyImage(eglDisplay, m_images[i]);
            m_images[i] = EGL_NO_IMAGE_KHR;
        }
    }
}

void LinuxDmaBufV1ClientBuffer::buffer_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

} // namespace KWaylandServer
