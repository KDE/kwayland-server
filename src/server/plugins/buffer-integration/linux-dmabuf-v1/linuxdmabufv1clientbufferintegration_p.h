/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbufferintegration_p.h"
#include "kwaylandeglext.h"

#include <QHash>
#include <QVector>

namespace KWaylandServer
{

class LinuxDmaBufV1ClientBuffer;
class LinuxDmaBufV1;

class LinuxDmaBufV1EglFunctions
{
public:
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT = nullptr;
    PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT = nullptr;
};

class LinuxDmaBufV1ClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit LinuxDmaBufV1ClientBufferIntegration(QObject *parent = nullptr);
    ~LinuxDmaBufV1ClientBufferIntegration() override;

    bool initialize(Display *display) override;

    EGLDisplay eglDisplay() const;
    bool supportsModifiers() const;
    QHash<uint32_t, QVector<uint64_t>> supportedModifiers() const;
    Display *display() const;
    LinuxDmaBufV1EglFunctions *functions() const;

private:
    QVector<uint32_t> supportedFormats() const;
    QVector<uint64_t> supportedModifiers(uint32_t format) const;

    Display *m_display;
    QScopedPointer<LinuxDmaBufV1> m_linuxDmaBufV1;
    QScopedPointer<LinuxDmaBufV1EglFunctions> m_functions;
    QHash<uint32_t, QVector<uint64_t>> m_supportedModifiers;
    bool m_supportsModifiers = false;
};

} // namespace KWaylandServer
