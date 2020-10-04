/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "eglimageclientbufferintegration_p.h"
#include "display.h"
#include "eglimageclientbuffer_p.h"
#include "logging.h"

namespace KWaylandServer
{

EglImageClientBufferIntegration::EglImageClientBufferIntegration(QObject *parent)
    : ClientBufferIntegration(parent)
    , m_functions(new EglImageFunctions)
{
}

EglImageClientBufferIntegration::~EglImageClientBufferIntegration()
{
}

EglImageFunctions *EglImageClientBufferIntegration::functions() const
{
    return m_functions.data();
}

bool EglImageClientBufferIntegration::initialize(Display *display)
{
    m_display = display;

    EGLDisplay eglDisplay = display->rendererInterface()->eglDisplay();
    if (!eglDisplay) {
        qCDebug(KWS_EGLIMAGE) << "Cannot setup the integration because EGLDisplay is missing";
        return false;
    }

    m_functions->eglQueryWaylandBufferWL =
            reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
    if (!m_functions->eglQueryWaylandBufferWL) {
        qCWarning(KWS_EGLIMAGE) << "Failed to load eglQueryWaylandBufferWL";
        return false;
    }

    return true;
}

EGLDisplay EglImageClientBufferIntegration::eglDisplay() const
{
    return m_display->rendererInterface()->eglDisplay();
}

Display *EglImageClientBufferIntegration::display() const
{
    return m_display;
}

ClientBuffer *EglImageClientBufferIntegration::createBuffer(::wl_resource *resource)
{
    EGLint format;
    if (m_functions->eglQueryWaylandBufferWL(eglDisplay(), resource, EGL_TEXTURE_FORMAT, &format)) {
        return new EglImageClientBuffer(this, resource);
    }
    return nullptr;
}

} // namespace KWaylandServer
