/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "surfacerole_p.h"
#include "surface_interface_p.h"
#include "surface_interface.h"

namespace KWaylandServer
{

SurfaceRole::SurfaceRole(SurfaceInterface *surface)
    : m_surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->role = this;
}

SurfaceRole::~SurfaceRole()
{
    // Lifetime of the surface role is not bounded to the associated surface.
    if (m_surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(m_surface);
        surfacePrivate->role = nullptr;
    }
}

void SurfaceRole::postCommit()
{
}

SurfaceRole *SurfaceRole::get(SurfaceInterface *surface)
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        return surfacePrivate->role;
    }

    return nullptr;
}

}
