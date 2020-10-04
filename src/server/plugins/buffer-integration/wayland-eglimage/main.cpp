/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "eglimageclientbufferintegration_p.h"
#include "clientbufferintegrationfactory_p.h"

namespace KWaylandServer
{

class EglImageClientBufferIntegrationFactory : public ClientBufferIntegrationFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ClientBufferIntegrationFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWaylandServer::ClientBufferIntegrationFactory)

public:
    ClientBufferIntegration *create(const QString &key) override;
};

ClientBufferIntegration *EglImageClientBufferIntegrationFactory::create(const QString &key)
{
    if (key.compare(QStringLiteral("eglimage"), Qt::CaseInsensitive) == 0) {
        return new EglImageClientBufferIntegration();
    }
    return nullptr;
}

} // namespace KWaylandServer

#include "main.moc"
