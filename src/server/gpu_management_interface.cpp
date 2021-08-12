/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "gpu_management_interface.h"

#include <qwayland-server-org-kde-kwin-gpu.h>
#include "display.h"

namespace KWaylandServer
{

class GpuManagementInterfacePrivate : public QtWaylandServer::org_kde_kwin_gpu_management
{
public:
    GpuManagementInterfacePrivate (Display *display, GpuManagementInterface *q, const QVector<GpuManagementInterface::Gpu> &gpus);

    GpuManagementInterface *q;
    QVector<GpuManagementInterface::Gpu> gpus;

    void sendList(Resource *resource);

private:
    void org_kde_kwin_gpu_management_bind_resource(Resource *resource) override;
    void org_kde_kwin_gpu_management_destroy(Resource *resource) override;

    void org_kde_kwin_gpu_management_release_fd(Resource *resource, const QString &identifier) override;
    void org_kde_kwin_gpu_management_switch_compositing(Resource *resource, const QString &identifier) override;

};

GpuManagementInterfacePrivate::GpuManagementInterfacePrivate (Display *display, GpuManagementInterface *q, const QVector<GpuManagementInterface::Gpu> &gpus)
    : QtWaylandServer::org_kde_kwin_gpu_management(*display, 1)
    , q(q)
    , gpus(gpus)
{
}

void GpuManagementInterfacePrivate::org_kde_kwin_gpu_management_bind_resource(Resource *resource)
{
    sendList(resource);
}

void GpuManagementInterfacePrivate::sendList(Resource *resource)
{
    for (const auto &gpu : gpus) {
        send_identifier(resource->handle, gpu.identifier);
        send_name(resource->handle, gpu.name);
        send_usage(resource->handle, static_cast<uint32_t>(gpu.usage));
        send_gpu_done(resource->handle);
    }
    send_done(resource->handle);
}

void GpuManagementInterfacePrivate::org_kde_kwin_gpu_management_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void GpuManagementInterfacePrivate::org_kde_kwin_gpu_management_release_fd(Resource *resource, const QString &identifier)
{
    Q_UNUSED(resource);
    Q_EMIT q->releaseFdRequested(identifier);
}

void GpuManagementInterfacePrivate::org_kde_kwin_gpu_management_switch_compositing(Resource *resource, const QString &identifier)
{
    Q_UNUSED(resource);
    Q_EMIT q->switchCompositingRequested(identifier);
}

GpuManagementInterface::GpuManagementInterface(Display *display, const QVector<Gpu> &gpus)
    : d(new GpuManagementInterfacePrivate (display, this, gpus))
{
}

GpuManagementInterface::~GpuManagementInterface()
{
}

QVector<GpuManagementInterface::Gpu> GpuManagementInterface::gpuList() const
{
    return d->gpus;
}

void GpuManagementInterface::setGpuList(const QVector<Gpu> &gpus)
{
    d->gpus = gpus;
    for (const auto &res : d->resourceMap()) {
        d->sendList(res);
    }
}

}
