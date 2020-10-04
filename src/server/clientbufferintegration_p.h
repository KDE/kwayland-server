/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbufferref.h"

#include <QObject>

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

class ClientBuffer;
class Display;

class KWAYLANDSERVER_EXPORT ClientBufferIntegration : public QObject
{
    Q_OBJECT

public:
    explicit ClientBufferIntegration(QObject *parent = nullptr);

    virtual bool initialize(Display *display) = 0;
    virtual ClientBuffer *createBuffer(wl_resource *resource);
};

} // namespace KWaylandServer
