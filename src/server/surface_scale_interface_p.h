/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-surface-scale-dev.h"

#include <QPointer>

namespace KWaylandServer
{
class SurfaceInterface;

class SurfaceScaleInterface : public QtWaylandServer::zwp_surface_scale_dev
{
public:
    SurfaceScaleInterface(SurfaceInterface *surface, wl_resource *resource);
    ~SurfaceScaleInterface() override;

    static SurfaceScaleInterface *get(SurfaceInterface *surface);

    QPointer<SurfaceInterface> surface;

protected:
    //    void zwp_surface_scale_dev_destroy_resource(Resource *resource) override;
    //    void zwp_surface_scale_dev_destroy(Resource *resource) override;
    void zwp_surface_scale_dev_set_client_scale(Resource *resource, wl_fixed_t scale) override;
};

} // namespace KWaylandServer
