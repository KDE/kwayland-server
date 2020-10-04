/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "qwayland-server-wl-eglstream-controller.h"

namespace KWaylandServer
{

class EglStreamClientBufferIntegration;

class EglStreamController : public QtWaylandServer::wl_eglstream_controller
{
public:
    explicit EglStreamController(EglStreamClientBufferIntegration *integration);

protected:
    void eglstream_controller_attach_eglstream_consumer(Resource *resource, wl_resource *surface, wl_resource *buffer) override;
    void eglstream_controller_attach_eglstream_consumer_attribs(Resource *resource, wl_resource *surface, wl_resource *buffer, wl_array *attribs) override;

private:
    EglStreamClientBufferIntegration *m_integration;
};

} // namespace KWaylandServer
