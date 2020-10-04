/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbufferintegration_p.h"
#include "kwaylandeglext.h"

namespace KWaylandServer
{

class EglImageClientBuffer;

class EglImageFunctions
{
public:
    PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBufferWL = nullptr;
};

class EglImageClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit EglImageClientBufferIntegration(QObject *parent = nullptr);
    ~EglImageClientBufferIntegration() override;

    EGLDisplay eglDisplay() const;
    EglImageFunctions *functions() const;
    Display *display() const;

    bool initialize(Display *display) override;
    ClientBuffer *createBuffer(::wl_resource *resource) override;

private:
    Display *m_display = nullptr;
    QScopedPointer<EglImageFunctions> m_functions;
};

} // namespace KWaylandServer
