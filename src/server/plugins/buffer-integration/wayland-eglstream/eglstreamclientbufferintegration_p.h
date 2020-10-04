/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clientbufferintegration_p.h"
#include "kwaylandeglext.h"

namespace KWaylandServer
{

class EglStreamClientBuffer;
class EglStreamController;

class EglStreamFunctions
{
public:
    PFNEGLCREATESTREAMATTRIBNVPROC eglCreateStreamAttribNV = nullptr;
    PFNEGLQUERYSTREAMATTRIBNVPROC eglQueryStreamAttribNV = nullptr;
    PFNEGLSTREAMCONSUMERACQUIREATTRIBNVPROC eglStreamConsumerAcquireAttribNV = nullptr;
    PFNEGLSTREAMCONSUMERRELEASEATTRIBNVPROC eglStreamConsumerReleaseAttribNV = nullptr;
    PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBufferWL = nullptr;
};

class EglStreamClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit EglStreamClientBufferIntegration(QObject *parent = nullptr);
    ~EglStreamClientBufferIntegration() override;

    bool initialize(Display *display) override;

    EGLDisplay eglDisplay() const;
    Display *display() const;
    EglStreamFunctions *functions() const;

private:
    Display *m_display = nullptr;
    QScopedPointer<EglStreamController> m_controller;
    QScopedPointer<EglStreamFunctions> m_functions;
};

} // namespace KWaylandServer
