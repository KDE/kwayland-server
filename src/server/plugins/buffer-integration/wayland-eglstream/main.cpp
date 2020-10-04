/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglstreamclientbufferintegration_p.h"
#include "clientbufferintegrationfactory_p.h"

namespace KWaylandServer
{

class EglStreamClientBufferIntegrationFactory : public ClientBufferIntegrationFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ClientBufferIntegrationFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWaylandServer::ClientBufferIntegrationFactory)

public:
    ClientBufferIntegration *create(const QString &key) override;
};

ClientBufferIntegration *EglStreamClientBufferIntegrationFactory::create(const QString &key)
{
    if (key.compare(QStringLiteral("eglstream"), Qt::CaseInsensitive) == 0) {
        return new EglStreamClientBufferIntegration();
    }
    return nullptr;
}

} // namespace KWaylandServer

#include "main.moc"
