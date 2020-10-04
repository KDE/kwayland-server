/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbufferref.h"

#include <QObject>

#include <wayland-server-core.h>

namespace KWaylandServer
{

//
//  W A R N I N G
//  -------------
//
// This file is part of the KWaylandServer API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of KWaylandServer.
//

class ClientBufferIntegration;

class KWAYLANDSERVER_EXPORT ClientBuffer : public QObject
{
    Q_OBJECT

public:
    ClientBuffer(ClientBufferIntegration *integration, wl_resource *handle);

    ::wl_resource *handle() const;
    ClientBufferIntegration *integration() const;

    bool isReferenced() const;
    bool isDestroyed() const;

    void ref();
    void unref();

    virtual ClientBufferRef::Origin origin() const = 0;
    virtual ClientBufferRef::TextureType textureType() const = 0;
    virtual QImage toImage();
    virtual GLuint toOpenGLTexture(int plane);
    virtual QSize size() const = 0;
    virtual bool hasAlphaChannel() const = 0;

    void markAsDestroyed();
    void markAsDirty(const QRegion &region, quint64 cursor, quint64 handle);

    quint64 damageCursor() const;
    quint64 damageHandle() const;

protected:
    QRegion m_damageRegion;
    quint64 m_damageCursor = 0;
    quint64 m_damageHandle = 0;

private:
    wl_resource *m_handle;
    ClientBufferIntegration *m_integration;
    int m_refCount = 0;
};

} // namespace KWaylandServer
