/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>
#include <QObject>

#include <variant>

struct wl_resource;
struct wl_client;

namespace KWaylandServer
{

class Display;
class XdgToplevelInterface;
class ClientConnection;

class XdgDBusAnnotationV1Interface;

using XdgDBusAnnotationV1Target = std::variant<XdgToplevelInterface*, ClientConnection*>;

class XdgDBusAnnotationManagerV1InterfacePrivate;

class KWAYLANDSERVER_EXPORT XdgDBusAnnotationManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit XdgDBusAnnotationManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgDBusAnnotationManagerV1Interface() override;

    XdgDBusAnnotationV1Interface* annotationFor(const QString& name, const XdgDBusAnnotationV1Target& of);

Q_SIGNALS:
    void annotationCreated(XdgDBusAnnotationV1Interface*);

private:
    QScopedPointer<XdgDBusAnnotationManagerV1InterfacePrivate> d;
};

class XdgDBusAnnotationV1InterfacePrivate;

class KWAYLANDSERVER_EXPORT XdgDBusAnnotationV1Interface : public QObject
{
    Q_OBJECT

public:
    ~XdgDBusAnnotationV1Interface() override;

    struct InterfaceAddress {
        QString serviceName;
        QString objectPath;
    };

    QString name() const;
    XdgDBusAnnotationV1Target target() const;

    InterfaceAddress address() const;

Q_SIGNALS:
    void addressChanged();

private:
    friend class XdgDBusAnnotationManagerV1InterfacePrivate;
    QScopedPointer<XdgDBusAnnotationV1InterfacePrivate> d;

    explicit XdgDBusAnnotationV1Interface(const XdgDBusAnnotationV1Target& tl, const QString& name, wl_resource *resource);
    friend class XdgDBusAnnotationManagerV1Interface;

};

};
