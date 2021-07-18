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

class XdgDBusAnnotationInterface;

using XdgDBusAnnotationTarget = std::variant<XdgToplevelInterface*, wl_client*>;

class KWAYLANDSERVER_EXPORT XdgDBusAnnotationManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit XdgDBusAnnotationManagerInterface(Display *display, QObject *parent = nullptr);
    ~XdgDBusAnnotationManagerInterface() override;

    XdgDBusAnnotationInterface* annotationFor(const QString& name, const XdgDBusAnnotationTarget& of);

Q_SIGNALS:
    void annotationCreated(XdgDBusAnnotationInterface*);

private:
    struct Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT XdgDBusAnnotationInterface : public QObject
{
    Q_OBJECT

public:
    ~XdgDBusAnnotationInterface() override;

    struct InterfaceAddress {
        QString serviceName;
        QString objectPath;
    };

    QString name() const;
    XdgDBusAnnotationTarget target() const;

    InterfaceAddress address() const;
    Q_SIGNAL void addressChanged();

private:
    struct Private;
    QScopedPointer<Private> d;

    explicit XdgDBusAnnotationInterface(const XdgDBusAnnotationTarget& tl, const QString& name, wl_resource *resource);
    friend class XdgDBusAnnotationManagerInterface;

};

};
