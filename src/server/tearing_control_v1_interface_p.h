/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "tearing_control_v1_interface.h"

#include <qwayland-server-tearing-control-unstable-v1.h>

namespace KWaylandServer
{

class TearingControlV1InterfacePrivate : public QtWaylandServer::zwp_tearing_control_v1
{
public:
    TearingControlV1InterfacePrivate(Display *display);

private:
    void zwp_tearing_control_v1_destroy(Resource *resource) override;
    void zwp_tearing_control_v1_get_tearing_control(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class SurfaceTearingControlV1InterfacePrivate : public QtWaylandServer::zwp_surface_tearing_control_v1
{
public:
    SurfaceTearingControlV1InterfacePrivate(wl_resource *resource);

    SurfaceTearingControlV1Interface::PresentationHint hint = SurfaceTearingControlV1Interface::PresentationHint::async;// FIXME
    SurfaceTearingControlV1Interface::PresentationHint pendingHint = SurfaceTearingControlV1Interface::PresentationHint::vsync;
    SurfaceTearingControlV1Interface *q = nullptr;
private:
    void zwp_surface_tearing_control_v1_set_presentation_hint(Resource *resource, uint32_t hint) override;
    void zwp_surface_tearing_control_v1_destroy(Resource *resource) override;
};

}
