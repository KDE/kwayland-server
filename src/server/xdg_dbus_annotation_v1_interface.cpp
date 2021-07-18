/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdg_dbus_annotation_v1_interface_p.h"

namespace KWaylandServer
{

XdgDBusAnnotationManagerInterface::XdgDBusAnnotationManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(this, display))
{
    d->init(*display, s_version);
}

XdgDBusAnnotationManagerInterface::~XdgDBusAnnotationManagerInterface()
{
}

XdgDBusAnnotationInterface* XdgDBusAnnotationManagerInterface::annotationFor(const QString& name, const XdgDBusAnnotationTarget& of)
{
    for (const auto& it : d->annotations) {
        if (it->target() == of && it->name() == name) {
            return it;
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

XdgDBusAnnotationInterface::XdgDBusAnnotationInterface(const XdgDBusAnnotationTarget& tl, const QString& name, wl_resource *resource)
    : QObject(nullptr)
    , d(new Private(this, name, tl, resource))
{

}

XdgDBusAnnotationInterface::~XdgDBusAnnotationInterface()
{

}

QString XdgDBusAnnotationInterface::name() const
{
    return d->name;
}

XdgDBusAnnotationTarget XdgDBusAnnotationInterface::target() const
{
    return d->target;
}

XdgDBusAnnotationInterface::InterfaceAddress XdgDBusAnnotationInterface::address() const
{
    return d->address;
}

};
