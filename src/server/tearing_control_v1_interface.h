/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

namespace KWaylandServer
{

class Display;
class TearingControlV1InterfacePrivate;
class SurfaceTearingControlV1InterfacePrivate;

class KWAYLANDSERVER_EXPORT TearingControlV1Interface : public QObject
{
    Q_OBJECT
public:
    explicit TearingControlV1Interface(Display *display, QObject *parent = nullptr);
    ~TearingControlV1Interface() override;

private:
    QScopedPointer<TearingControlV1InterfacePrivate> d;
};

class KWAYLANDSERVER_EXPORT SurfaceTearingControlV1Interface : public QObject
{
    Q_OBJECT
public:
    ~SurfaceTearingControlV1Interface() override;

    enum PresentationHint {
        vsync = 0,
        vsync_relaxed = 1,
        async = 2,
    };
    PresentationHint presentationHint() const;

    void commit();

private:
    explicit SurfaceTearingControlV1Interface(SurfaceTearingControlV1InterfacePrivate *d);
    friend class TearingControlV1InterfacePrivate;
    QScopedPointer<SurfaceTearingControlV1InterfacePrivate> d;
};

}
