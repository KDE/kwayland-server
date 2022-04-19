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
class ScalerInterfacePrivate : public QtWaylandServer::wp_fractional_scale_manager_v1
{
protected:
    void wp_fractional_scale_manager_v1_destroy(Resource *resource) override;
    void wp_fractional_scale_manager_v1_get_fractional_scale(Resource *resource, uint32_t id, wl_resource *surface) override;
};

void ScalerInterfacePrivate::wp_fractional_scale_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ScalerInterfacePrivate::wp_fractional_scale_manager_v1_get_fractional_scale(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);

    SurfaceScaleInterface *scaleIface = SurfaceScaleInterface::get(surface);
    if (scaleIface) {
        //        wl_resource_post_error(resource->handle, error_viewport_exists, "the specified surface already has a ");
        return;
    }

    wl_resource *surfaceScalerResource = wl_resource_create(resource->client(), &wp_fractional_scale_v1_interface, resource->version(), id);

    new SurfaceScaleInterface(surface, surfaceScalerResource);
}

SurfaceScaleInterface::SurfaceScaleInterface(SurfaceInterface *surface, wl_resource *resource)
    : QtWaylandServer::wp_fractional_scale_v1(resource)
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
