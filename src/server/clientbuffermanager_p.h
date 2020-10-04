/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QHash>
#include <QObject>

struct wl_resource;

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

class ClientBuffer;
class ClientBufferIntegration;
class Display;
struct ClientBufferDestroyListener;

class KWAYLANDSERVER_EXPORT ClientBufferManager : public QObject
{
    Q_OBJECT

public:
    explicit ClientBufferManager(Display *display, QObject *parent = nullptr);
    ~ClientBufferManager() override;

    void initialize();
    ClientBuffer *bufferForResource(wl_resource *resource);

    void registerClientBuffer(ClientBuffer *buffer);
    void unregisterClientBuffer(ClientBuffer *buffer);

private:
    Display *m_display;
    QHash<::wl_resource *, ClientBuffer *> m_resourceToBuffer;
    QHash<ClientBuffer *, ClientBufferDestroyListener *> m_bufferToListener;
    QList<ClientBufferIntegration *> m_bufferIntegrations;
    bool m_isInitialized = false;
};

} // namespace KWaylandServer
