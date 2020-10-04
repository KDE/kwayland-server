/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>
#include <QString>

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

#define ClientBufferIntegrationFactory_iid "org.kde.kwaylandserver.ClientBufferIntegrationFactory.5.0"

class ClientBufferIntegration;

class KWAYLANDSERVER_EXPORT ClientBufferIntegrationFactory : public QObject
{
    Q_OBJECT

public:
    virtual ClientBufferIntegration *create(const QString &key) = 0;
};

} // namespace KWaylandServer

Q_DECLARE_INTERFACE(KWaylandServer::ClientBufferIntegrationFactory, ClientBufferIntegrationFactory_iid)
