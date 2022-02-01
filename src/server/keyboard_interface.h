/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{
class ClientConnection;
class SeatInterface;
class SurfaceInterface;
class KeyboardInterfacePrivate;

enum class KeyboardKeyState : quint32;

/**
 * @brief Resource for the wl_keyboard interface.
 */
class KWAYLANDSERVER_EXPORT KeyboardInterface : public QObject
{
    Q_OBJECT
public:
    ~KeyboardInterface() override;

    /**
     * @returns the focused SurfaceInterface on this keyboard resource, if any.
     */
    SurfaceInterface *focusedSurface() const;

    /**
     * @returns The key repeat in character per second
     */
    qint32 keyRepeatRate() const;
    /**
     * @returns The delay on key press before starting repeating keys
     */
    qint32 keyRepeatDelay() const;
    void setKeymap(const QByteArray &content);

    /**
     * Sets the key repeat information to be forwarded to all bound keyboards.
     *
     * To disable key repeat set a @p charactersPerSecond of @c 0.
     *
     * Requires wl_seat version 4.
     *
     * @param charactersPerSecond The characters per second rate, value of @c 0 disables key repeating
     * @param delay The delay on key press before starting repeating keys
     */
    void setRepeatInfo(qint32 charactersPerSecond, qint32 delay);

    void sendKey(quint32 key, KeyboardKeyState state);
    void sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group);

    bool containsAdditionalClient(ClientConnection *client) const;
    void addAdditionalClient(ClientConnection *client);
    void removeAdditionalClient(ClientConnection *client);

private:
    void setFocusedSurface(SurfaceInterface *surface, quint32 serial);
    friend class SeatInterface;
    friend class KeyboardInterfacePrivate;
    explicit KeyboardInterface(SeatInterface *seat);

    QScopedPointer<KeyboardInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::KeyboardInterface *)
