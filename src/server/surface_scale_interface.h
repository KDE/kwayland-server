/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

namespace KWaylandServer
{
class Display;
class ScalerInterfacePrivate;

/**
 * The ViewporterInterface is an extension that allows clients to crop and scale surfaces.
 *
 * The ViewporterInterface extensions provides a way for Wayland clients to crop and scale their
 * surfaces. This effectively breaks the direct connection between the buffer and the surface size.
 *
 * ViewporterInterface corresponds to the Wayland interface @c wp_viewporter.
 */
class KWAYLANDSERVER_EXPORT ScalerInterface : public QObject
{
    Q_OBJECT

public:
    explicit ScalerInterface(Display *display, QObject *parent = nullptr);
    ~ScalerInterface() override;

private:
    QScopedPointer<ScalerInterfacePrivate> d;
};

} // namespace KWaylandServer
