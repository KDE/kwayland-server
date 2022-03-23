/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "surface_scale_interface.h"
#include "display.h"
#include "surface_interface_p.h"
#include "surface_scale_interface_p.h"

static const int s_version = 1;

namespace KWaylandServer
{
class ScalerInterfacePrivate : public QtWaylandServer::zwp_scaler_dev
{
protected:
    void zwp_scaler_dev_destroy(Resource *resource) override;
    void zwp_scaler_dev_get_surface_scale(Resource *resource, uint32_t id, wl_resource *surface) override;
};

void ScalerInterfacePrivate::zwp_scaler_dev_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ScalerInterfacePrivate::zwp_scaler_dev_get_surface_scale(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);

    // TODO
    //    SurfaceScaleInterface *scaleIface = SurfaceScaleInterface::get(surface);
    //    if (scaleIface) {
    //        wl_resource_post_error(resource->handle, error_viewport_exists, "the specified surface already has a ");
    //        return;
    //    }

    wl_resource *surfaceScalerResource = wl_resource_create(resource->client(), &zwp_scaler_dev_interface, resource->version(), id);

    new SurfaceScaleInterface(surface, surfaceScalerResource);
}

SurfaceScaleInterface::SurfaceScaleInterface(SurfaceInterface *surface, wl_resource *resource)
    : QtWaylandServer::zwp_surface_scale_dev(resource)
    , surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->scalerExtension = this;
}

SurfaceScaleInterface::~SurfaceScaleInterface()
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->scalerExtension = nullptr;
    }
}

SurfaceScaleInterface *SurfaceScaleInterface::get(SurfaceInterface *surface)
{
    return SurfaceInterfacePrivate::get(surface)->scalerExtension;
}

void SurfaceScaleInterface::zwp_surface_scale_dev_set_client_scale(Resource *resource, wl_fixed_t scale)
{
    Q_UNUSED(resource);
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->pending.bufferScale = wl_fixed_to_double(scale);
    surfacePrivate->pending.bufferScaleIsSet = true;
}

ScalerInterface::ScalerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ScalerInterfacePrivate)
{
    d->init(*display, s_version);
}

ScalerInterface::~ScalerInterface()
{
}

} // namespace KWaylandServer
