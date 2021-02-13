/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "keyboard_interface_p.h"
#include "datadevice_interface.h"
#include "display.h"
#include "logging.h"
#include "primaryselectiondevice_v1_interface.h"
#include "seat_interface.h"
#include "seat_interface_p.h"
#include "surface_interface.h"
#include "compositor_interface.h"
// Qt
#include <QTemporaryFile>
#include <QVector>

#include <unistd.h>

namespace KWaylandServer
{

KeyboardInterfacePrivate::KeyboardInterfacePrivate(SeatInterface *s)
    : seat(s)
{
}

void KeyboardInterfacePrivate::keyboard_bind_resource(Resource *resource)
{
    if (resource->version() >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
        send_repeat_info(resource->handle, keyRepeat.charactersPerSecond, keyRepeat.delay);
    }
    if (!keymap.isNull()) {
        send_keymap(resource->handle, keymap_format::keymap_format_xkb_v1, keymap->handle(), keymap->size());
    }
}

QList<KeyboardInterfacePrivate::Resource *> KeyboardInterfacePrivate::keyboardsForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

void KeyboardInterfacePrivate::sendLeave(SurfaceInterface *surface, quint32 serial)
{
    const QList<Resource *> keyboards = keyboardsForClient(surface->client());
    for (Resource *keyboardResource : keyboards) {
        send_leave(keyboardResource->handle, serial, surface->resource());
    }
}

void KeyboardInterfacePrivate::sendEnter(SurfaceInterface *surface, quint32 serial)
{
    const auto states = pressedKeys();
    QByteArray data = QByteArray::fromRawData(
        reinterpret_cast<const char*>(states.constData()),
        sizeof(quint32) * states.size()
    );

    const QList<Resource *> keyboards = keyboardsForClient(surface->client());
    for (Resource *keyboardResource : keyboards) {
        send_enter(keyboardResource->handle, serial, surface->resource(), data);
    }

    sendModifiers();
}

void KeyboardInterface::setKeymap(const QByteArray &content)
{
    if (content.isNull()) {
        return;
    }
    QScopedPointer<QTemporaryFile> tmp{new QTemporaryFile(this)};
    if (!tmp->open()) {
        return;
    }
    unlink(tmp->fileName().toUtf8().constData());
    if (!tmp->resize(content.size())) {
        return;
    }
    uchar *address = tmp->map(0, content.size());
    if (!address) {
        return;
    }
    if (qstrncpy(reinterpret_cast<char*>(address), content.constData(), content.size() + 1) == nullptr) {
        return;
    }
    tmp->unmap(address);

    d->sendKeymap(tmp->handle(), content.size());
    d->keymap.swap(tmp);
}

void KeyboardInterfacePrivate::sendKeymap(int fd, quint32 size)
{
    const QList<Resource *> keyboards = resourceMap().values();
    for (Resource *keyboardResource : keyboards) {
        send_keymap(keyboardResource->handle, keymap_format::keymap_format_xkb_v1, fd, size);
    }
}

void KeyboardInterfacePrivate::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial)
{
    if (!focusedSurface) {
        return;
    }
    const QList<Resource *> keyboards = keyboardsForClient(focusedSurface->client());
    for (Resource *keyboardResource : keyboards) {
        send_modifiers(keyboardResource->handle, serial, depressed, latched, locked, group);
    }
}

bool KeyboardInterfacePrivate::updateKey(quint32 key, State state)
{
    auto it = states.find(key);
    if (it == states.end()) {
        states.insert(key, state);
        return true;
    }
    if (it.value() == state) {
        return false;
    }
    it.value() = state;
    return true;
}

KeyboardInterface::KeyboardInterface(SeatInterface *seat)
    : d(new KeyboardInterfacePrivate(seat))
{
}

KeyboardInterface::~KeyboardInterface() = default;

void KeyboardInterfacePrivate::sendModifiers()
{
    sendModifiers(modifiers.depressed, modifiers.latched, modifiers.locked, modifiers.group, modifiers.serial);
}

void KeyboardInterface::setFocusedSurface(SurfaceInterface *surface, quint32 serial)
{
    SeatInterface::Private *seatPrivate = d->seat->d_func();
    if (d->focusedSurface == surface) {
        return;
    }

    if (d->focusedSurface) {
        d->sendLeave(d->focusedSurface, serial);
        disconnect(d->destroyConnection);
    }

    d->focusedSurface = surface;
    if (!d->focusedSurface) {
        return;
    }
    d->destroyConnection = connect(d->focusedSurface, &SurfaceInterface::aboutToBeDestroyed, this, [this] {
        CompositorInterface *compositor = d->focusedSurface->compositor();
        d->sendLeave(d->focusedSurface, compositor->display()->nextSerial());
        d->focusedSurface = nullptr;
    });

    d->sendEnter(d->focusedSurface, serial);

    // selection?
    const QVector<DataDeviceInterface *> dataDevices = seatPrivate->dataDevicesForSurface(surface);
    seatPrivate->globalKeyboard.focus.selections = dataDevices;
    for (auto dataDevice : dataDevices) {
        if (seatPrivate->currentSelection) {
            dataDevice->sendSelection(seatPrivate->currentSelection);
        } else {
            dataDevice->sendClearSelection();
        }
    }
    // primary selection
    QVector<PrimarySelectionDeviceV1Interface *> primarySelectionDevices;
    for (auto selection : qAsConst(seatPrivate->primarySelectionDevices)) {
        if (selection->client() == *surface->client()) {
            primarySelectionDevices << selection;
        }
    }

    seatPrivate->globalKeyboard.focus.primarySelections = primarySelectionDevices;
    for (auto primaryDataDevice : qAsConst(primarySelectionDevices)) {
        if (seatPrivate->currentSelection) {
            primaryDataDevice->sendSelection(seatPrivate->currentPrimarySelection);
        } else {
            primaryDataDevice->sendClearSelection();
        }
    }
}

QVector<quint32> KeyboardInterfacePrivate::pressedKeys() const
{
    QVector<quint32> keys;
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        if (it.value() == State::Pressed) {
            keys << it.key();
        }
    }
    return keys;
}

void KeyboardInterface::sendPressed(quint32 key)
{
    if (!d->focusedSurface) {
        return;
    }

    if (!d->updateKey(key, KeyboardInterfacePrivate::State::Pressed)) {
        return;
    }

    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->keyboardsForClient(d->focusedSurface->client());
    const quint32 serial = d->seat->d_func()->nextSerial();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        d->send_key(keyboardResource->handle, serial, d->seat->timestamp(), key, KeyboardInterfacePrivate::key_state::key_state_pressed);
    }
}

void KeyboardInterface::sendReleased(quint32 key)
{
    if (!d->focusedSurface) {
        return;
    }

    if (!d->updateKey(key, KeyboardInterfacePrivate::State::Released)) {
        return;
    }

    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->keyboardsForClient(d->focusedSurface->client());
    const quint32 serial = d->seat->d_func()->nextSerial();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        d->send_key(keyboardResource->handle, serial, d->seat->timestamp(), key, KeyboardInterfacePrivate::key_state::key_state_released);
    }
}

void KeyboardInterface::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    bool changed = false;
#define UPDATE( value ) \
    if (d->modifiers.value != value) { \
        d->modifiers.value = value; \
        changed = true; \
    }
    UPDATE(depressed)
    UPDATE(latched)
    UPDATE(locked)
    UPDATE(group)
    if (!changed) {
        return;
    }

    if (!d->focusedSurface) {
        return;
    }

    d->modifiers.serial = d->seat->d_func()->nextSerial();
    d->sendModifiers(depressed, latched, locked, group, d->modifiers.serial);
}

void KeyboardInterface::setRepeatInfo(qint32 charactersPerSecond, qint32 delay)
{
    d->keyRepeat.charactersPerSecond = qMax(charactersPerSecond, 0);
    d->keyRepeat.delay = qMax(delay, 0);
    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->resourceMap().values();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        if (keyboardResource->version() >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
            d->send_repeat_info(keyboardResource->handle, d->keyRepeat.charactersPerSecond, d->keyRepeat.delay);
        }
    }
}

SurfaceInterface *KeyboardInterface::focusedSurface() const
{
    return d->focusedSurface;
}

qint32 KeyboardInterface::keyRepeatDelay() const
{
    return d->keyRepeat.delay;
}

qint32 KeyboardInterface::keyRepeatRate() const
{
    return d->keyRepeat.charactersPerSecond;
}

}
