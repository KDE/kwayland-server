/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgdbusannotation_v1_interface_p.h"

namespace KWaylandServer
{
XdgDBusAnnotationManagerV1Interface::XdgDBusAnnotationManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgDBusAnnotationManagerV1InterfacePrivate(this, display))
{
    d->init(*display, s_version);
}

XdgDBusAnnotationManagerV1Interface::~XdgDBusAnnotationManagerV1Interface()
{
}

XdgDBusAnnotationV1Interface *XdgDBusAnnotationManagerV1Interface::annotationFor(const QString &name, const XdgDBusAnnotationV1Target &of)
{
    for (const auto &it : d->annotations) {
        if (it->target() == of && it->name() == name) {
            return it;
        }
    }

    return nullptr;
}

XdgDBusAnnotationV1Interface::XdgDBusAnnotationV1Interface(const XdgDBusAnnotationV1Target &tl, const QString &name, wl_resource *resource)
    : QObject(nullptr)
    , d(new XdgDBusAnnotationV1InterfacePrivate(this, name, tl, resource))
{
}

XdgDBusAnnotationV1Interface::~XdgDBusAnnotationV1Interface()
{
}

QString XdgDBusAnnotationV1Interface::name() const
{
    return d->name;
}

XdgDBusAnnotationV1Target XdgDBusAnnotationV1Interface::target() const
{
    return d->target;
}

XdgDBusAnnotationV1Interface::InterfaceAddress XdgDBusAnnotationV1Interface::address() const
{
    return d->address;
}

};
