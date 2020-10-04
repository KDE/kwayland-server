/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "linuxdmabufv1clientbufferintegration_p.h"
#include "display.h"
#include "linuxdmabufv1clientbuffer_p.h"
#include "linuxdmabufv1interface_p.h"
#include "logging.h"
#include "rendererinterface.h"

namespace KWaylandServer
{

LinuxDmaBufV1ClientBufferIntegration::LinuxDmaBufV1ClientBufferIntegration(QObject *parent)
    : ClientBufferIntegration(parent)
    , m_functions(new LinuxDmaBufV1EglFunctions)
{
}

LinuxDmaBufV1ClientBufferIntegration::~LinuxDmaBufV1ClientBufferIntegration()
{
}

bool LinuxDmaBufV1ClientBufferIntegration::supportsModifiers() const
{
    return m_supportsModifiers;
}

EGLDisplay LinuxDmaBufV1ClientBufferIntegration::eglDisplay() const
{
    return m_display->rendererInterface()->eglDisplay();
}

QHash<uint32_t, QVector<uint64_t>> LinuxDmaBufV1ClientBufferIntegration::supportedModifiers() const
{
    return m_supportedModifiers;
}

Display *LinuxDmaBufV1ClientBufferIntegration::display() const
{
    return m_display;
}

LinuxDmaBufV1EglFunctions *LinuxDmaBufV1ClientBufferIntegration::functions() const
{
    return m_functions.data();
}

bool LinuxDmaBufV1ClientBufferIntegration::initialize(Display *display)
{
    m_display = display;

    EGLDisplay eglDisplay = display->rendererInterface()->eglDisplay();
    if (!eglDisplay) {
        qCDebug(KWS_LINUXDMABUF) << "The EGL Display is unavailable";
        return false;
    }

    const QByteArrayList requiredExtensions {
        QByteArrayLiteral("EGL_EXT_image_dma_buf_import"),
    };

    const char *extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    for (const QByteArray &requiredExtension : requiredExtensions) {
        if (!strstr(extensions, requiredExtension.constData())) {
            qCDebug(KWS_LINUXDMABUF) << requiredExtension << "is unsupported";
            return false;
        }
    }

    m_functions->eglQueryDmaBufFormatsEXT =
            reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
    if (!m_functions->eglQueryDmaBufFormatsEXT) {
        qCWarning(KWS_LINUXDMABUF) << "Failed to load eglQueryDmaBufFormatsEXT() function";
        return false;
    }

    m_functions->eglQueryDmaBufModifiersEXT =
            reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT"));
    if (!m_functions->eglQueryDmaBufModifiersEXT) {
        qCWarning(KWS_LINUXDMABUF) << "Failed to load eglQueryDmaBufModifiersEXT() function";
        return false;
    }

    m_supportsModifiers = strstr(extensions, "EGL_EXT_image_dma_buf_import_modifiers");

    const QVector<uint32_t> availableFormats = supportedFormats();
    for (const uint32_t &format : availableFormats) {
        m_supportedModifiers.insert(format, supportedModifiers(format));
    }

    m_linuxDmaBufV1.reset(new LinuxDmaBufV1(this));

    return true;
}

QVector<uint32_t> LinuxDmaBufV1ClientBufferIntegration::supportedFormats() const
{
    EGLDisplay display = m_display->rendererInterface()->eglDisplay();
    EGLint count;
    if (!m_functions->eglQueryDmaBufFormatsEXT(display, 0, nullptr, &count)) {
        qCWarning(KWS_LINUXDMABUF) << "Failed to query the total number of dmabuf formats";
        return QVector<uint32_t>();
    }

    QVector<uint32_t> formats(count);
    if (!m_functions->eglQueryDmaBufFormatsEXT(display, count, (EGLint *)formats.data(), &count)) {
        qCWarning(KWS_LINUXDMABUF) << "Failed to query supported dmabuf formats";
        return QVector<uint32_t>();
    }

    return formats;
}

QVector<uint64_t> LinuxDmaBufV1ClientBufferIntegration::supportedModifiers(uint32_t format) const
{
    if (!m_supportsModifiers) {
        return QVector<uint64_t>();
    }

    EGLDisplay display = m_display->rendererInterface()->eglDisplay();
    EGLint count;
    if (!m_functions->eglQueryDmaBufModifiersEXT(display, format, 0, nullptr, nullptr, &count)) {
        qCWarning(KWS_LINUXDMABUF) << "Failed to query number of dmabuf modifiers for format" << format;
        return QVector<uint64_t>();
    }

    QVector<uint64_t> modifiers(count);
    if (!m_functions->eglQueryDmaBufModifiersEXT(display, format, count, modifiers.data(), nullptr, &count)) {
        qCWarning(KWS_LINUXDMABUF) << "Failed to query modifiers for dmabuf format" << format;
        return QVector<uint64_t>();
    }

    return modifiers;
}

} // namespace KWaylandServer
