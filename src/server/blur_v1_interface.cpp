/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "blur_v1_interface.h"
#include "region_interface.h"
#include "display.h"
#include "surface_interface_p.h"

#include "qwayland-server-ext-blur-unstable-v1.h"

namespace KWaylandServer
{

static const quint32 s_version = 1;

class BlurV1Interface : public QtWaylandServer::zext_blur_v1
{
public:
    BlurV1Interface(SurfaceInterface *surface, struct ::wl_resource *resource);
    ~BlurV1Interface() override;

    static BlurV1Interface *get(SurfaceInterface *surface);

    QPointer<SurfaceInterface> surface;

protected:
    void zext_blur_v1_destroy_resource(Resource *resource) override;
    void zext_blur_v1_destroy(Resource *resource) override;
    void zext_blur_v1_set_region(Resource *resource, struct ::wl_resource *region_resource) override;
};

class BlurManagerV1InterfacePrivate : public QtWaylandServer::zext_blur_manager_v1
{
public:
    explicit BlurManagerV1InterfacePrivate(Display *display);

protected:
    void zext_blur_manager_v1_destroy(Resource *resource) override;
    void zext_blur_manager_v1_get_blur(Resource *resource, uint32_t id,
                                       struct ::wl_resource *surface_resource) override;
};

BlurManagerV1InterfacePrivate::BlurManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::zext_blur_manager_v1(*display, s_version)
{
}

void BlurManagerV1InterfacePrivate::zext_blur_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void BlurManagerV1InterfacePrivate::zext_blur_manager_v1_get_blur(Resource *resource, uint32_t id,
                                                                  struct ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    BlurV1Interface *blur = BlurV1Interface::get(surface);

    if (blur) {
        wl_resource_post_error(resource->handle, error_blur_exists,
                               "the specified surface already has a blur");
        return;
    }

    wl_resource *blurResource = wl_resource_create(resource->client(), &zext_blur_v1_interface,
                                                   resource->version(), id);

    new BlurV1Interface(surface, blurResource);
}

BlurManagerV1Interface::BlurManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new BlurManagerV1InterfacePrivate(display))
{
}

BlurManagerV1Interface::~BlurManagerV1Interface()
{
}

BlurV1Interface::BlurV1Interface(SurfaceInterface *surface, struct ::wl_resource *resource)
    : QtWaylandServer::zext_blur_v1(resource)
    , surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->blurV1Extension = this;
}

BlurV1Interface::~BlurV1Interface()
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->blurV1Extension = nullptr;
    }
}

BlurV1Interface *BlurV1Interface::get(SurfaceInterface *surface)
{
    return SurfaceInterfacePrivate::get(surface)->blurV1Extension;
}

void BlurV1Interface::zext_blur_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void BlurV1Interface::zext_blur_v1_destroy(Resource *resource)
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->pending.blurRegion = QRegion();
        surfacePrivate->pending.blurRegionIsSet = true;
    }

    wl_resource_destroy(resource->handle);
}

void BlurV1Interface::zext_blur_v1_set_region(Resource *resource, struct ::wl_resource *region_resource)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, error_no_surface,
                               "the wl_surface for this blur no longer exists");
        return;
    }

    const RegionInterface *region = RegionInterface::get(region_resource);

    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->pending.blurRegion = region ? region->region() : infiniteRegion();
    surfacePrivate->pending.blurRegionIsSet = true;
}

} // namespace KWaylandServer
