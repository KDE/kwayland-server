/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-wp-fractional-scale-v1.h"

#include <QPointer>

namespace KWaylandServer
{
class SurfaceInterface;

class SurfaceScaleInterface : public QtWaylandServer::wp_fractional_scale_v1
{
public:
    SurfaceScaleInterface(SurfaceInterface *surface, wl_resource *resource);
    ~SurfaceScaleInterface() override;

    static SurfaceScaleInterface *get(SurfaceInterface *surface);

    QPointer<SurfaceInterface> surface;

protected:
    // dtor blah
};

} // namespace KWaylandServer
