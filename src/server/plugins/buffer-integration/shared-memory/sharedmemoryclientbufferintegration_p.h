/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbufferintegration_p.h"

namespace KWaylandServer
{

class SharedMemoryClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit SharedMemoryClientBufferIntegration(QObject *parent = nullptr);
    ~SharedMemoryClientBufferIntegration() override;

    Display *display() const;

    bool initialize(Display *display) override;
    ClientBuffer *createBuffer(::wl_resource *resource) override;

private:
    Display *m_display = nullptr;
};

} // namespace KWaylandServer
