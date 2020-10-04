/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "sharedmemoryclientbufferintegration_p.h"
#include "display.h"
#include "sharedmemoryclientbuffer_p.h"

#include <wayland-server-core.h>

namespace KWaylandServer
{

SharedMemoryClientBufferIntegration::SharedMemoryClientBufferIntegration(QObject *parent)
    : ClientBufferIntegration(parent)
{
}

SharedMemoryClientBufferIntegration::~SharedMemoryClientBufferIntegration()
{
}

Display *SharedMemoryClientBufferIntegration::display() const
{
    return m_display;
}

bool SharedMemoryClientBufferIntegration::initialize(Display *display)
{
    m_display = display;

    return !wl_display_init_shm(*display);
}

ClientBuffer *SharedMemoryClientBufferIntegration::createBuffer(::wl_resource *resource)
{
    if (wl_shm_buffer_get(resource)) {
        return new SharedMemoryClientBuffer(resource, this);
    }
    return nullptr;
}

} // namespace KWaylandServer
