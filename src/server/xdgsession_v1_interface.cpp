/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgsession_v1_interface.h"
#include "display.h"
#include "logging.h"
#include "xdgshell_interface.h"
#include "xdgshell_interface_p.h"

#include <QCborMap>
#include <QCborValue>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QPointer>
#include <QUuid>

#include "qwayland-server-xdg-session-unstable-v1.h"

namespace KWaylandServer
{

static const int s_version = 1;

class XdgSessionManagerV1InterfacePrivate : public QtWaylandServer::zxdg_session_manager_v1
{
public:
    static XdgSessionManagerV1InterfacePrivate *get(XdgSessionManagerV1Interface *q);

    explicit XdgSessionManagerV1InterfacePrivate(XdgSessionManagerV1Interface *q);

    void registerSession(XdgToplevelSessionV1Interface *state);
    void unregisterSession(XdgToplevelSessionV1Interface *state);

    XdgSessionManagerV1Interface *q;
    QHash<QString, XdgToplevelSessionV1Interface *> handleToSession;
    QPointer<XdgSessionStorageV1> storage;

protected:
    void zxdg_session_manager_v1_destroy(Resource *resource) override;
    void zxdg_session_manager_v1_get_toplevel_session(Resource *resource, uint32_t id,
                                                      const QString &handle,
                                                      struct ::wl_resource *toplevel_resource) override;
};

class XdgToplevelSessionV1InterfacePrivate : public QtWaylandServer::zxdg_toplevel_session_v1
{
public:
    explicit XdgToplevelSessionV1InterfacePrivate(XdgToplevelSessionV1Interface *q,
                                                  XdgSessionManagerV1Interface *manager,
                                                  XdgToplevelInterface *toplevel,
                                                  const QString &surfaceId);

    XdgToplevelSessionV1Interface *q;
    XdgSessionManagerV1Interface *manager;
    QPointer<XdgToplevelInterface> toplevel;
    QString handle;

protected:
    void zxdg_toplevel_session_v1_bind_resource(Resource *resource) override;
    void zxdg_toplevel_session_v1_destroy_resource(Resource *resource) override;
    void zxdg_toplevel_session_v1_destroy(Resource *resource) override;
    void zxdg_toplevel_session_v1_remove(Resource *resource) override;
};

XdgSessionManagerV1InterfacePrivate *XdgSessionManagerV1InterfacePrivate::get(XdgSessionManagerV1Interface *q)
{
    return q->d.data();
}

XdgSessionManagerV1InterfacePrivate::XdgSessionManagerV1InterfacePrivate(XdgSessionManagerV1Interface *q)
    : q(q)
{
}

void XdgSessionManagerV1InterfacePrivate::registerSession(XdgToplevelSessionV1Interface *session)
{
    handleToSession.insert(session->handle(), session);
}

void XdgSessionManagerV1InterfacePrivate::unregisterSession(XdgToplevelSessionV1Interface *session)
{
    handleToSession.remove(session->handle());
}

void XdgSessionManagerV1InterfacePrivate::zxdg_session_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgSessionManagerV1InterfacePrivate::zxdg_session_manager_v1_get_toplevel_session(Resource *resource,
                                                                                       uint32_t id,
                                                                                       const QString &handle,
                                                                                       struct ::wl_resource *toplevel_resource)
{
    XdgToplevelInterface *toplevel = XdgToplevelInterface::get(toplevel_resource);

    if (toplevel->session()) {
        wl_resource_post_error(resource->handle,
                               QtWaylandServer::zxdg_toplevel_session_v1::error_already_constructed,
                               "the xdg_toplevel surface already has a session object");
        return;
    }

    if (toplevel->isConfigured()) {
        wl_resource_post_error(resource->handle,
                               QtWaylandServer::zxdg_toplevel_session_v1::error_unconfigured_buffer,
                               "the xdg_toplevel surface is already configured");
        return;
    }

    QString requestedHandle = handle;
    if (requestedHandle.isEmpty()) {
        requestedHandle = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (handleToSession.contains(requestedHandle)) {
        wl_resource_post_error(resource->handle,
                               QtWaylandServer::zxdg_toplevel_session_v1::error_in_use,
                               "session object is already in use");
        return;
    }

    wl_resource *stateResource = wl_resource_create(resource->client(),
                                                    &zxdg_toplevel_session_v1_interface,
                                                    resource->version(), id);
    if (!stateResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    new XdgToplevelSessionV1Interface(q, toplevel, requestedHandle, stateResource);
}

XdgToplevelSessionV1InterfacePrivate::XdgToplevelSessionV1InterfacePrivate(XdgToplevelSessionV1Interface *q,
                                                                           XdgSessionManagerV1Interface *manager,
                                                                           XdgToplevelInterface *toplevel,
                                                                           const QString &handle)
    : q(q)
    , manager(manager)
    , toplevel(toplevel)
    , handle(handle)
{
}

void XdgToplevelSessionV1InterfacePrivate::zxdg_toplevel_session_v1_bind_resource(Resource *resource)
{
    send_session_id(resource->handle, handle);
}

void XdgToplevelSessionV1InterfacePrivate::zxdg_toplevel_session_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void XdgToplevelSessionV1InterfacePrivate::zxdg_toplevel_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgToplevelSessionV1InterfacePrivate::zxdg_toplevel_session_v1_remove(Resource *resource)
{
    XdgSessionStorageV1 *storage = manager->storage();
    if (storage) {
        storage->remove(handle);
    }
    wl_resource_destroy(resource->handle);
}

XdgSessionManagerV1Interface::XdgSessionManagerV1Interface(Display *display,
                                                           XdgSessionStorageV1 *storage,
                                                           QObject *parent)
    : QObject(parent)
    , d(new XdgSessionManagerV1InterfacePrivate(this))
{
    d->storage = storage;
    d->init(*display, s_version);
}

XdgSessionManagerV1Interface::~XdgSessionManagerV1Interface()
{
}

XdgSessionStorageV1 *XdgSessionManagerV1Interface::storage() const
{
    return d->storage;
}

XdgToplevelSessionV1Interface::XdgToplevelSessionV1Interface(XdgSessionManagerV1Interface *manager,
                                                             XdgToplevelInterface *toplevel,
                                                             const QString &handle, wl_resource *resource)
    : d(new XdgToplevelSessionV1InterfacePrivate(this, manager, toplevel, handle))
{
    d->init(resource); // Called after the state handle has been initialized.

    XdgSessionManagerV1InterfacePrivate *managerPrivate = XdgSessionManagerV1InterfacePrivate::get(manager);
    managerPrivate->registerSession(this);

    XdgToplevelInterfacePrivate *toplevelPrivate = XdgToplevelInterfacePrivate::get(toplevel);
    toplevelPrivate->session = this;
}

XdgToplevelSessionV1Interface::~XdgToplevelSessionV1Interface()
{
    if (d->manager) {
        XdgSessionManagerV1InterfacePrivate::get(d->manager)->unregisterSession(this);
    }
    if (d->toplevel) {
        XdgToplevelInterfacePrivate::get(d->toplevel)->session = nullptr;
    }
}

XdgToplevelInterface *XdgToplevelSessionV1Interface::toplevel() const
{
    return d->toplevel;
}

QString XdgToplevelSessionV1Interface::handle() const
{
    return d->handle;
}

QVariant XdgToplevelSessionV1Interface::read(const QString &key, const QVariant &defaultValue) const
{
    const XdgSessionStorageV1 *storage = d->manager->storage();
    if (!storage) {
        qCWarning(KWAYLAND_SERVER) << "Reading session data without backing storage";
        return QVariant();
    }
    const QVariant data = storage->read(d->handle, key);
    if (data.isValid()) {
        return data;
    } else {
        return defaultValue;
    }
}

void XdgToplevelSessionV1Interface::write(const QString &key, const QVariant &value)
{
    XdgSessionStorageV1 *storage = d->manager->storage();
    if (storage) {
        storage->write(d->handle, key, value);
    } else {
        qCWarning(KWAYLAND_SERVER) << "Writing session data without backing storage";
    }
}

XdgSessionStorageV1::XdgSessionStorageV1(QObject *parent)
    : QObject(parent)
{
}

class XdgCborSessionStorageV1Private
{
public:
    void load();
    void save();

    QCborMap rootObject;
    QString fileName;
};

void XdgCborSessionStorageV1Private::load()
{
    if (!QFileInfo::exists(fileName)) {
        return;
    }

    QFile device(fileName);
    if (device.open(QIODevice::ReadOnly)) {
        rootObject = QCborValue::fromCbor(device.readAll()).toMap();
    } else {
        qCWarning(KWAYLAND_SERVER) << "Failed to load storage:" << device.errorString();
    }
}

void XdgCborSessionStorageV1Private::save()
{
    const QDir storageFolder(QFileInfo(fileName).absolutePath());
    if (!storageFolder.exists()) {
        storageFolder.mkpath(QStringLiteral("."));
    }

    QFile device(fileName);
    if (device.open(QIODevice::WriteOnly)) {
        device.write(rootObject.toCborValue().toCbor());
    } else {
        qCWarning(KWAYLAND_SERVER) << "Failed to dump storage:" << device.errorString();
    }
}

XdgCborSessionStorageV1::XdgCborSessionStorageV1(const QString &fileName, QObject *parent)
    : XdgSessionStorageV1(parent)
    , d(new XdgCborSessionStorageV1Private)
{
    d->fileName = fileName;
    d->load();
}

XdgCborSessionStorageV1::~XdgCborSessionStorageV1()
{
    d->save();
}

void XdgCborSessionStorageV1::remove(const QString &sessionId)
{
    d->rootObject.remove(sessionId);
}

QVariant XdgCborSessionStorageV1::read(const QString &sessionId, const QString &key) const
{
    QByteArray data = d->rootObject[sessionId][key].toByteArray();

    QDataStream stream(&data, QIODevice::ReadOnly);
    QVariant result;
    stream >> result;

    return result;
}

void XdgCborSessionStorageV1::write(const QString &sessionId, const QString &key, const QVariant &value)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << value;

    d->rootObject[sessionId][key] = QCborValue(data);
}

void XdgCborSessionStorageV1::save()
{
    d->save();
}

} // namespace KWaylandServer
