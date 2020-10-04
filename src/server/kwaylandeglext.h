/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <epoxy/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WAYLAND_BUFFER_WL                     0x31D5
#define EGL_WAYLAND_PLANE_WL                      0x31D6
#define EGL_TEXTURE_Y_U_V_WL                      0x31D7
#define EGL_TEXTURE_Y_UV_WL                       0x31D8
#define EGL_TEXTURE_Y_XUXV_WL                     0x31D9
#define EGL_TEXTURE_EXTERNAL_WL                   0x31DA
#define EGL_WAYLAND_Y_INVERTED_WL                 0x31DB

typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);

#endif // EGL_WL_bind_wayland_display

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT                     0x3270
#define EGL_LINUX_DRM_FOURCC_EXT                  0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT                 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT             0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT              0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT                 0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT             0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT              0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT                 0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT             0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT              0x327A
#endif // EGL_EXT_image_dma_buf_import

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE3_FD_EXT                 0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT             0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT              0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT        0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT        0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT        0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT        0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT        0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT        0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT        0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT        0x344A
#endif // EGL_EXT_image_dma_buf_import_modifiers

#ifndef EGL_WL_wayland_eglstream
#define EGL_WAYLAND_EGLSTREAM_WL                  0x334B
#endif // EGL_WL_wayland_eglstream

#ifndef EGL_NV_stream_attrib
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMATTRIBNVPROC) (EGLDisplay dpy, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib *value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif // EGL_NV_stream_attrib

#ifndef EGL_EXT_stream_acquire_mode
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREATTRIBEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif // EGL_EXT_stream_acquire_mode
