/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>
#include <QVariant>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class XdgCborSessionStorageV1Private;
class XdgSessionManagerV1InterfacePrivate;
class XdgToplevelInterface;
class XdgToplevelSessionV1InterfacePrivate;

/**
 * The XdgSessionStorageV1 class represents the storage for the compositor's session data.
 *
 * XdgSessionStorageV1 stores toplevel surface session data such as the frame geometry, the
 * maximize mode, etc. No restrictions are imposed on the data type, the compositor can store
 * data of any kind in the storage, for example QRect, QSize, QString, etc.
 *
 * Note that it is the responsibility of the compositor to decide when the storage must be
 * sync'ed.
 */
class KWAYLANDSERVER_EXPORT XdgSessionStorageV1 : public QObject
{
    Q_OBJECT

public:
    explicit XdgSessionStorageV1(QObject *parent = nullptr);

    virtual void remove(const QString &sessionId) = 0;
    virtual QVariant read(const QString &sessionId, const QString &key) const = 0;
    virtual void write(const QString &sessionId, const QString &key, const QVariant &value) = 0;
    virtual void save() = 0;
};

/**
 * The XdgCborSessionStorageV1 class represents a session storage backed by a KConfig.
 */
class KWAYLANDSERVER_EXPORT XdgCborSessionStorageV1 : public XdgSessionStorageV1
{
    Q_OBJECT

public:
    explicit XdgCborSessionStorageV1(const QString &fileName, QObject *parent = nullptr);
    ~XdgCborSessionStorageV1() override;

    void remove(const QString &sessionId) override;
    QVariant read(const QString &sessionId, const QString &key) const override;
    void write(const QString &sessionId, const QString &key, const QVariant &value) override;
    void save() override;

private:
    QScopedPointer<XdgCborSessionStorageV1Private> d;
};

/**
 * The XdgSessionManagerV1Interface compositor extension that allows clients to create sessions
 * for toplevel surfaces that persist across compositor and application restarts.
 *
 * The XdgSessionManagerV1Interface corresponds to the Wayland interface @c zxdg_session_manager_v1.
 */
class KWAYLANDSERVER_EXPORT XdgSessionManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgSessionManagerV1Interface(Display *display, XdgSessionStorageV1 *storage, QObject *parent = nullptr);
    ~XdgSessionManagerV1Interface() override;

    /**
     * Returns the backing storage for the compositor's session data.
     */
    XdgSessionStorageV1 *storage() const;

private:
    QScopedPointer<XdgSessionManagerV1InterfacePrivate> d;
    friend class XdgSessionManagerV1InterfacePrivate;
};

/**
 * The XdgToplevelSessionV1Interface class represents a session for an xdg_toplevel surface.
 *
 * The XdgToplevelSessionV1Interface corresponds to the Wayland interface @c zxdg_toplevel_session_v1.
 */
class KWAYLANDSERVER_EXPORT XdgToplevelSessionV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit XdgToplevelSessionV1Interface(XdgSessionManagerV1Interface *manager,
                                           XdgToplevelInterface *toplevel,
                                           const QString &handle, wl_resource *resource);
    ~XdgToplevelSessionV1Interface() override;

    /**
     * Returns the XdgToplevelInterface object associated with this session object.
     */
    XdgToplevelInterface *toplevel() const;

    /**
     * Returns the handle that uniquely identifies this toplevel session object.
     */
    QString handle() const;

    /**
     * Returns the value for the property @a key. If the session storage doesn't contain any
     * property with the specified key, this function returns @a defaultValue.
     */
    QVariant read(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * Sets the value of property @a key to @a value. If the key already exists, the previous
     * value is overwritten.
     */
    void write(const QString &key, const QVariant &value);

private:
    QScopedPointer<XdgToplevelSessionV1InterfacePrivate> d;
};

} // namespace KWaylandServer
