/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "keyboardgrab.h"

// auto-generated
#include "qwayland-server-wayland.h"

namespace KWaylandServer
{

class InputMethodKeyboardV1 : public QtWaylandServer::wl_keyboard
{
public:
    explicit InputMethodKeyboardV1(::wl_resource *resource);
};

class InputMethodKeyboardGrabV1 : public QObject, public KeyboardGrab
{
    Q_OBJECT

public:
    InputMethodKeyboardGrabV1(SeatInterface *seat, InputMethodKeyboardV1 *keyboard, QObject *parent);

    void cancel() override;

    void handleFocus(SurfaceInterface *surface, quint32 serial) override;
    void handleKey(quint32 keyCode, KeyboardKeyState state) override;
    void handleModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group) override;

private:
    QScopedPointer<InputMethodKeyboardV1> m_keyboard;
};

} // namespace KWaylandServer
