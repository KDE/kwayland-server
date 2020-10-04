/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbufferintegration_p.h"

namespace KWaylandServer
{

ClientBufferIntegration::ClientBufferIntegration(QObject *parent)
    : QObject(parent)
{
}

ClientBuffer *ClientBufferIntegration::createBuffer(wl_resource *resource)
{
    Q_UNUSED(resource)
    return nullptr;
}

} // namespace KWaylandServer
