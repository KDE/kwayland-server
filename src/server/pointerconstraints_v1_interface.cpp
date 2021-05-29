/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointerconstraints_v1_interface.h"
#include "display.h"
#include "pointer_interface.h"
#include "pointerconstraints_v1_interface_p.h"
#include "region_interface_p.h"
#include "surface_interface_p.h"

namespace KWaylandServer
{

static const int s_version = 1;

PointerConstraintsV1InterfacePrivate::PointerConstraintsV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_pointer_constraints_v1(*display, s_version)
{
}

static QRegion regionFromResource(::wl_resource *resource)
{
    const RegionInterface *region = RegionInterface::get(resource);
    return region ? region->region() : QRegion();
}

void PointerConstraintsV1InterfacePrivate::zwp_pointer_constraints_v1_lock_pointer(Resource *resource,
                                                                                   uint32_t id,
                                                                                   ::wl_resource *surface_resource,
                                                                                   ::wl_resource *pointer_resource,
                                                                                   ::wl_resource *region_resource,
                                                                                   uint32_t lifetime)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (!surface) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid surface");
        return;
    }

    if (surface->lockedPointer() || surface->confinedPointer()) {
        wl_resource_post_error(resource->handle, error_already_constrained,
                               "the surface is already constrained");
        return;
    }

    if (lifetime != lifetime_oneshot && lifetime != lifetime_persistent) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "unknown lifetime %d", lifetime);
        return;
    }

    wl_resource *lockedPointerResource = wl_resource_create(resource->client(),
                                                            &zwp_locked_pointer_v1_interface,
                                                            resource->version(), id);
    if (!lockedPointerResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto lockedPointer = new LockedPointerV1Interface(surface,
                                                      LockedPointerV1Interface::LifeTime(lifetime),
                                                      regionFromResource(region_resource),
                                                      lockedPointerResource);

    SurfaceInterfacePrivate::get(surface)->installPointerConstraint(lockedPointer);
}

void PointerConstraintsV1InterfacePrivate::zwp_pointer_constraints_v1_confine_pointer(Resource *resource,
                                                                                      uint32_t id,
                                                                                      ::wl_resource *surface_resource,
                                                                                      ::wl_resource *pointer_resource,
                                                                                      ::wl_resource *region_resource,
                                                                                      uint32_t lifetime)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (!surface) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid surface");
        return;
    }

    if (lifetime != lifetime_oneshot && lifetime != lifetime_persistent) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "unknown lifetime %d", lifetime);
        return;
    }

    if (surface->lockedPointer() || surface->confinedPointer()) {
        wl_resource_post_error(resource->handle, error_already_constrained,
                               "the surface is already constrained");
        return;
    }

    wl_resource *confinedPointerResource = wl_resource_create(resource->client(),
                                                              &zwp_confined_pointer_v1_interface,
                                                              resource->version(), id);
    if (!confinedPointerResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto confinedPointer = new ConfinedPointerV1Interface(surface,
                                                          ConfinedPointerV1Interface::LifeTime(lifetime),
                                                          regionFromResource(region_resource),
                                                          confinedPointerResource);

    SurfaceInterfacePrivate::get(surface)->installPointerConstraint(confinedPointer);
}

void PointerConstraintsV1InterfacePrivate::zwp_pointer_constraints_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PointerConstraintsV1Interface::PointerConstraintsV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PointerConstraintsV1InterfacePrivate(display))
{
}

PointerConstraintsV1Interface::~PointerConstraintsV1Interface()
{
}

LockedPointerV1InterfacePrivate *LockedPointerV1InterfacePrivate::get(LockedPointerV1Interface *q)
{
    return q->d.data();
}

LockedPointerV1State *LockedPointerV1State::get(SurfaceState *state)
{
    if (!state->lockedPointerV1) {
        state->lockedPointerV1.reset(new LockedPointerV1State);
    }
    return static_cast<LockedPointerV1State *>(state->lockedPointerV1.data());
}

LockedPointerV1InterfacePrivate::LockedPointerV1InterfacePrivate(LockedPointerV1Interface *q,
                                                                 LockedPointerV1Interface::LifeTime lifeTime,
                                                                 SurfaceInterface *surface,
                                                                 const QRegion &region,
                                                                 ::wl_resource *resource)
    : QtWaylandServer::zwp_locked_pointer_v1(resource)
    , q(q)
    , lifeTime(lifeTime)
    , surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    LockedPointerV1State *current = LockedPointerV1State::get(&surfacePrivate->current);
    current->region = region;
}

void LockedPointerV1InterfacePrivate::applyState()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    auto current = LockedPointerV1State::get(&surfacePrivate->current);
    auto pending = LockedPointerV1State::get(&surfacePrivate->pending);

    if (pending->regionIsSet) {
        current->region = pending->region;
        pending->regionIsSet = false;
        Q_EMIT q->regionChanged();
    }
    if (pending->hintIsSet) {
        current->hint = pending->hint;
        pending->hintIsSet = false;
        Q_EMIT q->cursorPositionHintChanged();
    }
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_set_cursor_position_hint(Resource *resource,
                                                                                     wl_fixed_t surface_x,
                                                                                     wl_fixed_t surface_y)
{
    Q_UNUSED(resource)
    if (Q_UNLIKELY(!surface)) {
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    auto pending = LockedPointerV1State::get(&surfacePrivate->pending);
    pending->hint = QPointF(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y));
    pending->hintIsSet = true;
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_set_region(Resource *resource,
                                                                       ::wl_resource *region_resource)
{
    Q_UNUSED(resource)
    if (Q_UNLIKELY(!surface)) {
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    auto pending = LockedPointerV1State::get(&surfacePrivate->pending);
    pending->region = regionFromResource(region_resource);
    pending->regionIsSet = true;
}

LockedPointerV1Interface::LockedPointerV1Interface(SurfaceInterface *surface,
                                                   LifeTime lifeTime, const QRegion &region,
                                                   ::wl_resource *resource)
    : d(new LockedPointerV1InterfacePrivate(this, lifeTime, surface, region, resource))
{
}

LockedPointerV1Interface::~LockedPointerV1Interface()
{
}

LockedPointerV1Interface::LifeTime LockedPointerV1Interface::lifeTime() const
{
    return d->lifeTime;
}

QRegion LockedPointerV1Interface::region() const
{
    if (Q_UNLIKELY(!d->surface)) {
        return QRegion();
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
    return LockedPointerV1State::get(&surfacePrivate->current)->region;
}

QPointF LockedPointerV1Interface::cursorPositionHint() const
{
    if (Q_UNLIKELY(!d->surface)) {
        return QPointF(-1, -1);
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
    return LockedPointerV1State::get(&surfacePrivate->current)->hint;
}

bool LockedPointerV1Interface::isLocked() const
{
    return d->isLocked;
}

void LockedPointerV1Interface::setLocked(bool locked)
{
    if (Q_UNLIKELY(!d->surface)) {
        return;
    }
    if (d->isLocked == locked) {
        return;
    }
    if (!locked) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
        LockedPointerV1State::get(&surfacePrivate->current)->hint = QPointF(-1, -1);
    }
    d->isLocked = locked;
    if (d->isLocked) {
        d->send_locked();
    } else {
        d->send_unlocked();
    }
    Q_EMIT lockedChanged();
}

ConfinedPointerV1InterfacePrivate *ConfinedPointerV1InterfacePrivate::get(ConfinedPointerV1Interface *q)
{
    return q->d.data();
}

ConfinedPointerV1State *ConfinedPointerV1State::get(SurfaceState *state)
{
    if (!state->confinedPointerV1) {
        state->confinedPointerV1.reset(new ConfinedPointerV1State);
    }
    return static_cast<ConfinedPointerV1State *>(state->confinedPointerV1.data());
}

ConfinedPointerV1InterfacePrivate::ConfinedPointerV1InterfacePrivate(ConfinedPointerV1Interface *q,
                                                                     ConfinedPointerV1Interface::LifeTime lifeTime,
                                                                     SurfaceInterface *surface,
                                                                     const QRegion &region,
                                                                     ::wl_resource *resource)
    : QtWaylandServer::zwp_confined_pointer_v1(resource)
    , q(q)
    , lifeTime(lifeTime)
    , surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    auto current = LockedPointerV1State::get(&surfacePrivate->current);
    current->region = region;
}

void ConfinedPointerV1InterfacePrivate::applyState()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    auto current = ConfinedPointerV1State::get(&surfacePrivate->current);
    auto pending = ConfinedPointerV1State::get(&surfacePrivate->pending);

    if (pending->regionIsSet) {
        current->region = pending->region;
        pending->regionIsSet = false;
        Q_EMIT q->regionChanged();
    }
}

void ConfinedPointerV1InterfacePrivate::zwp_confined_pointer_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void ConfinedPointerV1InterfacePrivate::zwp_confined_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ConfinedPointerV1InterfacePrivate::zwp_confined_pointer_v1_set_region(Resource *resource,
                                                                           ::wl_resource *region_resource)
{
    Q_UNUSED(resource)
    if (Q_UNLIKELY(!surface)) {
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    auto pending = ConfinedPointerV1State::get(&surfacePrivate->pending);
    pending->region = regionFromResource(region_resource);
    pending->regionIsSet = true;
}

ConfinedPointerV1Interface::ConfinedPointerV1Interface(SurfaceInterface *surface,
                                                       LifeTime lifeTime, const QRegion &region,
                                                       ::wl_resource *resource)
    : d(new ConfinedPointerV1InterfacePrivate(this, lifeTime, surface, region, resource))
{
}

ConfinedPointerV1Interface::~ConfinedPointerV1Interface()
{
}

ConfinedPointerV1Interface::LifeTime ConfinedPointerV1Interface::lifeTime() const
{
    return d->lifeTime;
}

QRegion ConfinedPointerV1Interface::region() const
{
    if (Q_UNLIKELY(!d->surface)) {
        return QRegion();
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
    return ConfinedPointerV1State::get(&surfacePrivate->current)->region;
}

bool ConfinedPointerV1Interface::isConfined() const
{
    return d->isConfined;
}

void ConfinedPointerV1Interface::setConfined(bool confined)
{
    if (d->isConfined == confined) {
        return;
    }
    d->isConfined = confined;
    if (d->isConfined) {
        d->send_confined();
    } else {
        d->send_unconfined();
    }
    Q_EMIT confinedChanged();
}

} // namespace KWaylandServer
