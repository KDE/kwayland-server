/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbuffermanager_p.h"
#include "clientbuffer_p.h"
#include "clientbufferintegrationfactory_p.h"
#include "clientbufferintegration_p.h"
#include "logging.h"

#include <KPluginLoader>
#include <KPluginMetaData>

#include <QJsonArray>
#include <QJsonObject>

namespace KWaylandServer
{

class ClientBufferIntegrationPluginRegistry
{
public:
    ClientBufferIntegrationPluginRegistry();

    QStringList keys() const;
    ClientBufferIntegration *create(const QString &key) const;

private:
    QMultiHash<QString, ClientBufferIntegrationFactory *> m_availablePlugins;
};

Q_GLOBAL_STATIC(ClientBufferIntegrationPluginRegistry, pluginRegistry)

static QStringList keywordsForMetaData(const QJsonObject &metadata)
{
    const QJsonArray rawKeywords = metadata.value(QStringLiteral("MetaData")).toObject()
            .value(QStringLiteral("Keys")).toArray();
    QStringList keywords;
    for (const QJsonValue &value : rawKeywords) {
        if (value.isString()) {
            keywords.append(value.toString());
        }
    }
    return keywords;
}

ClientBufferIntegrationPluginRegistry::ClientBufferIntegrationPluginRegistry()
{
    const QVector<QStaticPlugin> staticPlugins = QPluginLoader::staticPlugins();
    for (const QStaticPlugin &staticPlugin : staticPlugins) {
        const QJsonObject metadata = staticPlugin.metaData();
        if (metadata.value(QLatin1String("IID")) != QLatin1String(ClientBufferIntegrationFactory_iid)) {
            continue;
        }
        const QStringList keywords = keywordsForMetaData(metadata);
        if (keywords.isEmpty()) {
            qCWarning(KWAYLAND_SERVER) << "No keywords for client buffer integration factory plugin";
            continue;
        }
        ClientBufferIntegrationFactory *factory =
                qobject_cast<ClientBufferIntegrationFactory *>(staticPlugin.instance());
        if (!factory) {
            qCWarning(KWAYLAND_SERVER) << "Invalid client buffer integration factory plugin";
            continue;
        }
        for (const QString &keyword : keywords) {
            m_availablePlugins.insert(keyword, factory);
        }
    }

    const QVector<KPluginMetaData> candidates =
            KPluginLoader::findPlugins(QStringLiteral("kwaylandserver/buffer-integration"));
    for (const KPluginMetaData &candidate : candidates) {
        QPluginLoader loader(candidate.fileName());
        const QJsonObject metadata = loader.metaData();
        const QStringList keywords = keywordsForMetaData(metadata);
        if (keywords.isEmpty()) {
            qCWarning(KWAYLAND_SERVER) << "No keywords for client buffer integration factory plugin";
            continue;
        }
        ClientBufferIntegrationFactory *factory =
                qobject_cast<ClientBufferIntegrationFactory *>(loader.instance());
        if (!factory) {
            qCWarning(KWAYLAND_SERVER) << "Invalid client buffer integration factory plugin";
            continue;
        }
        for (const QString &keyword : keywords) {
            if (m_availablePlugins.contains(keyword)) {
                qCWarning(KWAYLAND_SERVER) << keyword << "client buffer plugin already exists";
            } else {
                m_availablePlugins.insert(keyword, factory);
            }
        }
    }
}

QStringList ClientBufferIntegrationPluginRegistry::keys() const
{
    return m_availablePlugins.keys();
}

ClientBufferIntegration *ClientBufferIntegrationPluginRegistry::create(const QString &key) const
{
    ClientBufferIntegrationFactory *factory = m_availablePlugins.value(key);
    if (factory) {
        return factory->create(key);
    } else {
        qCWarning(KWAYLAND_SERVER) << "Unknown buffer integration plugin" << key;
        return nullptr;
    }
}

struct ClientBufferDestroyListener : wl_listener
{
    ClientBufferDestroyListener(ClientBufferManager *manager, ClientBuffer *buffer);
    ~ClientBufferDestroyListener();

    ClientBufferManager *manager;
};

void bufferDestroyCallback(wl_listener *listener, void *data)
{
    ClientBufferDestroyListener *destroyListener = static_cast<ClientBufferDestroyListener *>(listener);
    ClientBufferManager *manager = destroyListener->manager;

    ClientBuffer *buffer = manager->bufferForResource(static_cast<wl_resource *>(data));
    manager->unregisterClientBuffer(buffer);

    buffer->markAsDestroyed();
}

ClientBufferDestroyListener::ClientBufferDestroyListener(ClientBufferManager *manager, ClientBuffer *buffer)
    : manager(manager)
{
    notify = bufferDestroyCallback;

    link.prev = nullptr;
    link.next = nullptr;

    wl_resource_add_destroy_listener(buffer->handle(), this);
}

ClientBufferDestroyListener::~ClientBufferDestroyListener()
{
    wl_list_remove(&link);
}

ClientBufferManager::ClientBufferManager(Display *display, QObject *parent)
    : QObject(parent)
    , m_display(display)
{
}

ClientBufferManager::~ClientBufferManager()
{
    Q_ASSERT(m_resourceToBuffer.isEmpty());
    qDeleteAll(m_bufferIntegrations);
}

void ClientBufferManager::initialize()
{
    if (m_isInitialized) {
        return;
    }

    const QString environmentKeys = qEnvironmentVariable("KWAYLAND_SERVER_BUFFER_INTEGRATION");

    QStringList keys = environmentKeys.split(QStringLiteral(";"), Qt::SkipEmptyParts);
    if (keys.isEmpty()) {
        keys = pluginRegistry()->keys();
    }

    keys.removeDuplicates();

    for (const QString &key : qAsConst(keys)) {
        ClientBufferIntegration *bufferIntegration = pluginRegistry()->create(key);
        if (!bufferIntegration) {
            continue;
        }
        if (bufferIntegration->initialize(m_display)) {
            m_bufferIntegrations.append(bufferIntegration);
        } else {
            delete bufferIntegration;
        }
    }

    m_isInitialized = true;
}

ClientBuffer *ClientBufferManager::bufferForResource(wl_resource *resource)
{
    ClientBuffer *buffer = m_resourceToBuffer.value(resource);
    if (buffer) {
        return buffer;
    }

    for (ClientBufferIntegration *integration : qAsConst(m_bufferIntegrations)) {
        ClientBuffer *buffer = integration->createBuffer(resource);
        if (buffer) {
            registerClientBuffer(buffer);
            return buffer;
        }
    }
    return nullptr;
}

void ClientBufferManager::registerClientBuffer(ClientBuffer *buffer)
{
    m_resourceToBuffer.insert(buffer->handle(), buffer);
    m_bufferToListener.insert(buffer, new ClientBufferDestroyListener(this, buffer));
}

void ClientBufferManager::unregisterClientBuffer(ClientBuffer *buffer)
{
    Q_ASSERT_X(buffer->handle(), "unregisterClientBuffer", "buffer must have valid resource");
    m_resourceToBuffer.remove(buffer->handle());
    delete m_bufferToListener.take(buffer);
}

} // namespace KWaylandServer
