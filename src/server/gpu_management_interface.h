/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class Display;
class GpuManagementInterfacePrivate;

class KWAYLANDSERVER_EXPORT GpuManagementInterface : public QObject {
    Q_OBJECT
public:
    enum class Usage {
        CompositingActive = 1, // this gpu is currently used for compositing
        CompositingPreferred = 2, // this gpu will be switched to when possible
        FdUsed = 4, // the compositor has a fd for this gpu open
    };
    Q_ENUM(Usage);
    Q_DECLARE_FLAGS(UsageFlags, Usage);

    struct Gpu {
        QString identifier;
        QString name;
        UsageFlags usage;
    };

    GpuManagementInterface (Display *display, const QVector<Gpu> &gpus);
    ~GpuManagementInterface();

    QVector<Gpu> gpuList() const;
    void setGpuList(const QVector<Gpu> &gpus);

Q_SIGNALS:
    void releaseFdRequested(const QString &identifier);
    void switchCompositingRequested(const QString &identifier);

private:
    QScopedPointer<GpuManagementInterfacePrivate> d;
};

}
