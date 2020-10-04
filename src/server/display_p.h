/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>
#include <wayland-server-core.h>

#include <QList>
#include <QSocketNotifier>
#include <QString>
#include <QVector>

#include <epoxy/egl.h>

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

class ClientBufferManager;
class ClientConnection;
class Display;
class OutputInterface;
class OutputDeviceInterface;
class RendererInterface;
class SeatInterface;

class KWAYLANDSERVER_EXPORT DisplayPrivate
{
public:
    static DisplayPrivate *get(Display *display);
    DisplayPrivate(Display *q);

    void registerSocketName(const QString &socketName);

    Display *q;
    QSocketNotifier *socketNotifier = nullptr;
    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
    bool running = false;
    QList<OutputInterface *> outputs;
    QList<OutputDeviceInterface *> outputdevices;
    QVector<SeatInterface *> seats;
    QVector<ClientConnection *> clients;
    QStringList socketNames;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    ClientBufferManager *bufferManager = nullptr;
    RendererInterface *rendererInterface = nullptr;
};

} // namespace KWaylandServer
