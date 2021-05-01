/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "tearing_control_v1_interface_p.h"
#include "surface_interface_p.h"
#include "display.h"
#include "logging.h"

namespace KWaylandServer
{

static const quint32 s_version = 1;

TearingControlV1Interface::TearingControlV1Interface(Display *display, QObject *parent)
    : QObject(parent),
    d(new TearingControlV1InterfacePrivate(display))
{
}

TearingControlV1Interface::~TearingControlV1Interface()
{
}

TearingControlV1InterfacePrivate::TearingControlV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_tearing_control_v1(*display, s_version)
{
}

void TearingControlV1InterfacePrivate::zwp_tearing_control_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TearingControlV1InterfacePrivate::zwp_tearing_control_v1_get_tearing_control(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    wl_resource *handle = wl_resource_create(resource->client(), &zwp_surface_tearing_control_v1_interface, s_version, id);
    if (!handle) {
        wl_resource_post_no_memory(resource->handle);
    }
    auto surfaceInterface = SurfaceInterface::get(surface);
    if (!surfaceInterface) {
        // TODO error?
        return;
    }
    auto tearingControl = new SurfaceTearingControlV1Interface(new SurfaceTearingControlV1InterfacePrivate(handle));
    SurfaceInterfacePrivate::get(surfaceInterface)->setTearingControl(tearingControl);
}

SurfaceTearingControlV1Interface::SurfaceTearingControlV1Interface(SurfaceTearingControlV1InterfacePrivate *d)
    : d(d)
{
    d->q = this;
}

SurfaceTearingControlV1Interface::~SurfaceTearingControlV1Interface()
{
}

SurfaceTearingControlV1Interface::PresentationHint SurfaceTearingControlV1Interface::presentationHint() const
{
    return d->hint;
}

void SurfaceTearingControlV1Interface::commit()
{
    d->hint = d->pendingHint;
}

SurfaceTearingControlV1InterfacePrivate::SurfaceTearingControlV1InterfacePrivate(wl_resource *resource)
    : QtWaylandServer::zwp_surface_tearing_control_v1(resource)
{

}

void SurfaceTearingControlV1InterfacePrivate::zwp_surface_tearing_control_v1_set_presentation_hint(Resource *resource, uint32_t u32hint)
{
    Q_UNUSED(resource);
    if (u32hint > static_cast<uint32_t>(SurfaceTearingControlV1Interface::PresentationHint::async)) {
        // TODO error
    }
    pendingHint = static_cast<SurfaceTearingControlV1Interface::PresentationHint>(u32hint);
}

void SurfaceTearingControlV1InterfacePrivate::zwp_surface_tearing_control_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
    delete q;
}

}
