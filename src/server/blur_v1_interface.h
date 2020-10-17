/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class Display;
class BlurManagerV1InterfacePrivate;

/**
 * The BlurManagerV1Interface is an extension provides a way for clients to ask the compositor
 * to blur background behind their surfaces.
 *
 * The main purpose of the blur extension is to improve visuals of translucent surfaces, such as
 * docks, panels, notifications, and so on by blurring background behind them.
 *
 * BlurManagerV1Interface corresponds to the Wayland interface @c ext_blur_manager_v1.
 */
class KWAYLANDSERVER_EXPORT BlurManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit BlurManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~BlurManagerV1Interface() override;

private:
    QScopedPointer<BlurManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
