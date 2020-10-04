/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglstreamclientbufferintegration_p.h"
#include "display.h"
#include "eglstreamclientbuffer_p.h"
#include "eglstreamcontroller_p.h"
#include "logging.h"

namespace KWaylandServer
{

EglStreamClientBufferIntegration::EglStreamClientBufferIntegration(QObject *parent)
    : ClientBufferIntegration(parent)
    , m_functions(new EglStreamFunctions)
{
}

EglStreamClientBufferIntegration::~EglStreamClientBufferIntegration()
{
}

EGLDisplay EglStreamClientBufferIntegration::eglDisplay() const
{
    return m_display->rendererInterface()->eglDisplay();
}

Display *EglStreamClientBufferIntegration::display() const
{
    return m_display;
}

EglStreamFunctions *EglStreamClientBufferIntegration::functions() const
{
    return m_functions.data();
}

bool EglStreamClientBufferIntegration::initialize(Display *display)
{
    m_display = display;

    EGLDisplay eglDisplay = display->rendererInterface()->eglDisplay();
    if (!eglDisplay) {
        qCDebug(KWS_EGLSTREAM) << "The EGL Display is unavailable";
        return false;
    }

    const QByteArrayList requiredExtensions {
        QByteArrayLiteral("EGL_EXT_stream_acquire_mode"),
        QByteArrayLiteral("EGL_KHR_stream"),
        QByteArrayLiteral("EGL_KHR_stream_consumer_gltexture"),
        QByteArrayLiteral("EGL_NV_stream_attrib"),
        QByteArrayLiteral("EGL_WL_wayland_eglstream"),
    };

    const char *extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    for (const QByteArray &requiredExtension : requiredExtensions) {
        if (!strstr(extensions, requiredExtension.constData())) {
            qCDebug(KWS_EGLSTREAM) << requiredExtension << "is unsupported";
            return false;
        }
    }

    m_functions->eglCreateStreamAttribNV =
            reinterpret_cast<PFNEGLCREATESTREAMATTRIBNVPROC>(eglGetProcAddress("eglCreateStreamAttribNV"));
    if (!m_functions->eglCreateStreamAttribNV) {
        qCWarning(KWS_EGLSTREAM) << "Failed to load eglCreateStreamAttribNV() function";
        return false;
    }

    m_functions->eglQueryStreamAttribNV =
            reinterpret_cast<PFNEGLQUERYSTREAMATTRIBNVPROC>(eglGetProcAddress("eglQueryStreamAttribNV"));
    if (!m_functions->eglQueryStreamAttribNV) {
        qCWarning(KWS_EGLSTREAM) << "Failed to load eglQueryStreamAttribNV() function";
        return false;
    }

    m_functions->eglStreamConsumerAcquireAttribNV =
            reinterpret_cast<PFNEGLSTREAMCONSUMERACQUIREATTRIBNVPROC>(eglGetProcAddress("eglStreamConsumerAcquireAttribNV"));
    if (!m_functions->eglStreamConsumerAcquireAttribNV) {
        qCWarning(KWS_EGLSTREAM) << "Failed to load eglStreamConsumerAcquireAttribNV() function";
        return false;
    }

    m_functions->eglStreamConsumerReleaseAttribNV =
            reinterpret_cast<PFNEGLSTREAMCONSUMERRELEASEATTRIBNVPROC>(eglGetProcAddress("eglStreamConsumerReleaseAttribNV"));
    if (!m_functions->eglStreamConsumerReleaseAttribNV) {
        qCWarning(KWS_EGLSTREAM) << "Failed to load eglStreamConsumerReleaseAttribNV() function";
        return false;
    }

    m_functions->eglQueryWaylandBufferWL =
            reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
    if (!m_functions->eglQueryWaylandBufferWL) {
        qCWarning(KWS_EGLSTREAM) << "Failed to load eglQueryWaylandBufferWL() function";
        return false;
    }

    m_controller.reset(new EglStreamController(this));

    return true;
}

} // namespace KWaylandServer
