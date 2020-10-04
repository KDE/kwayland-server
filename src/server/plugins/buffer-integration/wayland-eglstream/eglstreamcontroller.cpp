/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglstreamcontroller_p.h"
#include "clientbuffermanager_p.h"
#include "display.h"
#include "display_p.h"
#include "eglstreamclientbufferintegration_p.h"
#include "eglstreamclientbuffer_p.h"

namespace KWaylandServer
{

static const quint32 s_version = 1;

EglStreamController::EglStreamController(EglStreamClientBufferIntegration *integration)
    : QtWaylandServer::wl_eglstream_controller(*integration->display(), s_version)
    , m_integration(integration)
{
}

void EglStreamController::eglstream_controller_attach_eglstream_consumer(Resource *resource, wl_resource *surface, wl_resource *buffer)
{
    wl_array noAttribs = { 0, 0, nullptr };
    eglstream_controller_attach_eglstream_consumer_attribs(resource, surface, buffer, &noAttribs);
}

void EglStreamController::eglstream_controller_attach_eglstream_consumer_attribs(Resource *resource, wl_resource *surface, wl_resource *buffer, wl_array *attribs)
{
    Q_UNUSED(resource)
    Q_UNUSED(surface)

    EglStreamClientBuffer *clientBuffer = new EglStreamClientBuffer(m_integration, buffer, attribs);

    DisplayPrivate *displayPrivate = DisplayPrivate::get(m_integration->display());
    displayPrivate->bufferManager->registerClientBuffer(clientBuffer);
}

} // namespace KWaylandServer
