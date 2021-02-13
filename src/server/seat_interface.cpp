/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "abstract_data_source.h"
#include "display_p.h"
#include "seat_interface.h"
#include "seat_interface_p.h"
#include "display.h"
#include "datadevice_interface.h"
#include "datadevice_interface_p.h"
#include "datasource_interface.h"
#include "datacontroldevice_v1_interface.h"
#include "datacontrolsource_v1_interface.h"
#include "keyboard_interface.h"
#include "keyboard_interface_p.h"
#include "logging.h"
#include "pointergestures_v1_interface_p.h"
#include "pointer_interface.h"
#include "pointer_interface_p.h"
#include "primaryselectiondevice_v1_interface.h"
#include "primaryselectionsource_v1_interface.h"
#include "relativepointer_v1_interface_p.h"
#include "subcompositor_interface.h"
#include "surface_interface.h"
#include "textinput_v2_interface_p.h"
#include "textinput_v3_interface_p.h"
#include "touch_interface_p.h"
#include "utils.h"
// linux
#include <config-kwaylandserver.h>
#if HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

#include <functional>

namespace KWaylandServer
{

static const int s_version = 5;

SeatInterfacePrivate *SeatInterfacePrivate::get(SeatInterface *seat)
{
    return seat->d.data();
}

SeatInterfacePrivate::SeatInterfacePrivate(SeatInterface *q, Display *display)
    : QtWaylandServer::wl_seat(*display, s_version)
    , q(q)
    , display(display)
{
    textInputV2 = new TextInputV2Interface(q);
    textInputV3 = new TextInputV3Interface(q);
}

void SeatInterfacePrivate::seat_bind_resource(Resource *resource)
{
    quint32 capabilities = {};

    if (pointer) {
        capabilities |= capability_pointer;
    }
    if (keyboard) {
        capabilities |= capability_keyboard;
    }
    if (touch) {
        capabilities |= capability_touch;
    }

    send_capabilities(resource->handle, capabilities);

    if (resource->version() >= WL_SEAT_NAME_SINCE_VERSION) {
        send_name(resource->handle, name);
    }
}

void SeatInterfacePrivate::seat_get_pointer(Resource *resource, uint32_t id)
{
    if (!pointer) {
        wl_resource_post_error(resource->handle, 0, "wl_seat has no wl_pointer capability");
        return;
    }

    PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer.data());
    pointerPrivate->add(resource->client(), id, resource->version());
}

void SeatInterfacePrivate::seat_get_keyboard(Resource *resource, uint32_t id)
{
    if (!keyboard) {
        wl_resource_post_error(resource->handle, 0, "wl_seat has no wl_keyboard capability");
        return;
    }

    KeyboardInterfacePrivate *keyboardPrivate = KeyboardInterfacePrivate::get(keyboard.data());
    keyboardPrivate->add(resource->client(), id, resource->version());

}

void SeatInterfacePrivate::seat_get_touch(Resource *resource, uint32_t id)
{
    if (!touch) {
        wl_resource_post_error(resource->handle, 0, "wl_seat has no wl_touch capability");
        return;
    }

    TouchInterfacePrivate *touchPrivate = TouchInterfacePrivate::get(touch.data());
    touchPrivate->add(resource->client(), id, resource->version());
}

void SeatInterfacePrivate::seat_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

SeatInterface::SeatInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new SeatInterfacePrivate(this, display))
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(d->display);
    displayPrivate->seats.append(this);
}

SeatInterface::~SeatInterface()
{
    if (d->display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->display);
        displayPrivate->seats.removeOne(this);
    }
}

void SeatInterfacePrivate::updatePointerButtonSerial(quint32 button, quint32 serial)
{
    auto it = globalPointer.buttonSerials.find(button);
    if (it == globalPointer.buttonSerials.end()) {
        globalPointer.buttonSerials.insert(button, serial);
        return;
    }
    it.value() = serial;
}

void SeatInterfacePrivate::updatePointerButtonState(quint32 button, Pointer::State state)
{
    auto it = globalPointer.buttonStates.find(button);
    if (it == globalPointer.buttonStates.end()) {
        globalPointer.buttonStates.insert(button, state);
        return;
    }
    it.value() = state;
}

quint32 SeatInterfacePrivate::nextSerial() const
{
    return display->nextSerial();
}

QVector<DataDeviceInterface *> SeatInterfacePrivate::dataDevicesForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return {};
    }
    QVector<DataDeviceInterface *> primarySelectionDevices;
    for (auto it = dataDevices.constBegin(); it != dataDevices.constEnd(); ++it) {
        if ((*it)->client() == *surface->client()) {
            primarySelectionDevices << *it;
        }
    }
    return primarySelectionDevices;
}

void SeatInterfacePrivate::registerDataDevice(DataDeviceInterface *dataDevice)
{
    Q_ASSERT(dataDevice->seat() == q);
    dataDevices << dataDevice;
    auto dataDeviceCleanup = [this, dataDevice] {
        dataDevices.removeOne(dataDevice);
        globalKeyboard.focus.selections.removeOne(dataDevice);
    };
    QObject::connect(dataDevice, &QObject::destroyed, q, dataDeviceCleanup);
    QObject::connect(dataDevice, &DataDeviceInterface::selectionChanged, q,
        [this, dataDevice] {
            updateSelection(dataDevice);
        }
    );
    QObject::connect(dataDevice, &DataDeviceInterface::selectionCleared, q,
        [this, dataDevice] {
            updateSelection(dataDevice);
        }
    );
    QObject::connect(dataDevice, &DataDeviceInterface::dragStarted, q,
        [this, dataDevice] {
            const auto dragSerial = dataDevice->dragImplicitGrabSerial();
            auto *dragSurface = dataDevice->origin();
            if (q->hasImplicitPointerGrab(dragSerial)) {
                drag.mode = Drag::Mode::Pointer;
                drag.sourcePointer = pointer.data();
                drag.transformation = globalPointer.focus.transformation;
            } else if (q->hasImplicitTouchGrab(dragSerial)) {
                drag.mode = Drag::Mode::Touch;
                drag.sourceTouch = touch.data();
                // TODO: touch transformation
            } else {
                // no implicit grab, abort drag
                return;
            }
            auto *originSurface = dataDevice->origin();
            const bool proxied = originSurface->dataProxy();
            if (!proxied) {
                // origin surface
                drag.target = dataDevice;
                drag.surface = originSurface;
                // TODO: transformation needs to be either pointer or touch
                drag.transformation = globalPointer.focus.transformation;
            }
            drag.source = dataDevice;
            drag.sourcePointer = pointer.data();
            drag.destroyConnection = QObject::connect(dataDevice, &DataDeviceInterface::aboutToBeDestroyed, q,
                [this] {
                    cancelDrag(display->nextSerial());
                }
            );
            if (dataDevice->dragSource()) {
                drag.dragSourceDestroyConnection = QObject::connect(dataDevice->dragSource(), &AbstractDataSource::aboutToBeDestroyed, q,
                    [this] {
                        cancelDrag(display->nextSerial());
                    }
                );
            } else {
                drag.dragSourceDestroyConnection = QMetaObject::Connection();
            }
            dataDevice->updateDragTarget(proxied ? nullptr : originSurface, dataDevice->dragImplicitGrabSerial());
            emit q->dragStarted();
            emit q->dragSurfaceChanged();
        }
    );
    // is the new DataDevice for the current keyoard focus?
    if (globalKeyboard.focus.surface) {
        // same client?
        if (*globalKeyboard.focus.surface->client() == dataDevice->client()) {
            globalKeyboard.focus.selections.append(dataDevice);
            if (currentSelection) {
                dataDevice->sendSelection(currentSelection);
            }
        }
    }
}

void SeatInterfacePrivate::registerDataControlDevice(DataControlDeviceV1Interface *dataDevice)
{
    Q_ASSERT(dataDevice->seat() == q);
    dataControlDevices << dataDevice;
    auto dataDeviceCleanup = [this, dataDevice] {
        dataControlDevices.removeOne(dataDevice);
    };
    QObject::connect(dataDevice, &QObject::destroyed, q, dataDeviceCleanup);

    QObject::connect(dataDevice, &DataControlDeviceV1Interface::selectionChanged, q,
                     [this, dataDevice] {
        // Special klipper workaround to avoid a race
        // If the mimetype x-kde-onlyReplaceEmpty is set, and we've had another update in the meantime, do nothing
        // See https://github.com/swaywm/wlr-protocols/issues/92
        if  (dataDevice->selection() && dataDevice->selection()->mimeTypes().contains(QLatin1String("application/x-kde-onlyReplaceEmpty")) &&
             currentSelection) {
            dataDevice->selection()->cancel();
            return;
        }
        q->setSelection(dataDevice->selection());
    }
    );

    QObject::connect(dataDevice, &DataControlDeviceV1Interface::selectionCleared, q,
        [this, dataDevice] {
            Q_UNUSED(dataDevice);
            q->setSelection(nullptr);
        }
    );
    if (currentSelection) {
        dataDevice->sendSelection(currentSelection);
    }
}

void SeatInterfacePrivate::registerPrimarySelectionDevice(PrimarySelectionDeviceV1Interface *primarySelectionDevice)
{
    Q_ASSERT(primarySelectionDevice->seat() == q);

    primarySelectionDevices << primarySelectionDevice;
    auto dataDeviceCleanup = [this, primarySelectionDevice] {
        primarySelectionDevices.removeOne(primarySelectionDevice);
        globalKeyboard.focus.primarySelections.removeOne(primarySelectionDevice);
    };
    QObject::connect(primarySelectionDevice, &QObject::destroyed, q, dataDeviceCleanup);
    QObject::connect(primarySelectionDevice, &PrimarySelectionDeviceV1Interface::selectionChanged, q,
        [this, primarySelectionDevice] {
            updatePrimarySelection(primarySelectionDevice);
        }
    );
    QObject::connect(primarySelectionDevice, &PrimarySelectionDeviceV1Interface::selectionCleared, q,
        [this, primarySelectionDevice] {
            updatePrimarySelection(primarySelectionDevice);
        }
    );
    // is the new DataDevice for the current keyoard focus?
    if (globalKeyboard.focus.surface) {
        // same client?
        if (*globalKeyboard.focus.surface->client() == primarySelectionDevice->client()) {
            globalKeyboard.focus.primarySelections.append(primarySelectionDevice);
            if (currentPrimarySelection) {
                primarySelectionDevice->sendSelection(currentPrimarySelection);
            }
        }
    }
}

void SeatInterfacePrivate::cancelDrag(quint32 serial)
{
    if (drag.target) {
        drag.target->updateDragTarget(nullptr, serial);
        drag.target = nullptr;
    }
    endDrag(serial);
}

void SeatInterfacePrivate::endDrag(quint32 serial)
{
    QObject::disconnect(drag.destroyConnection);
    QObject::disconnect(drag.dragSourceDestroyConnection);

    DataDeviceInterface *dragTargetDevice = drag.target;
    DataDeviceInterface *dragSourceDevice = drag.source;
    DataSourceInterface *dragSource = dragSourceDevice ? dragSourceDevice->dragSource() : nullptr;
    if (dragSource) {
        // TODO: Also check the current drag-and-drop action.
        if (dragTargetDevice && dragSource->isAccepted()) {
            dragTargetDevice->drop();
            dragSource->dropPerformed();
        } else {
            if (wl_resource_get_version(dragSource->resource()) >=
                    WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION) {
                dragSource->cancel();
            }
        }
    }

    if (dragTargetDevice) {
        dragTargetDevice->updateDragTarget(nullptr, serial);
    }
    if (dragSourceDevice) {
        auto devicePrivate = DataDeviceInterfacePrivate::get(dragSourceDevice);
        devicePrivate->endDrag();
    }

    drag = Drag();
    emit q->dragSurfaceChanged();
    emit q->dragEnded();
}

void SeatInterfacePrivate::updateSelection(DataDeviceInterface *dataDevice)
{
    // if the update is from the focussed window we should inform the active client
    if (!(globalKeyboard.focus.surface && (*globalKeyboard.focus.surface->client() == dataDevice->client()))) {
        return;
    }
    q->setSelection(dataDevice->selection());
}

void SeatInterfacePrivate::updatePrimarySelection(PrimarySelectionDeviceV1Interface *primarySelectionDevice)
{
    // if the update is from the focussed window we should inform the active client
    if (!(globalKeyboard.focus.surface && (*globalKeyboard.focus.surface->client() == primarySelectionDevice->client()))) {
        return;
    }
    q->setPrimarySelection(primarySelectionDevice->selection());
}

void SeatInterfacePrivate::sendCapabilities()
{
    quint32 capabilities = 0;
    if (pointer) {
        capabilities |= SeatInterfacePrivate::capability_pointer;
    }
    if (keyboard) {
        capabilities |= SeatInterfacePrivate::capability_keyboard;
    }
    if (touch) {
        capabilities |= SeatInterfacePrivate::capability_touch;
    }

    const auto seatResources = resourceMap();
    for (SeatInterfacePrivate::Resource *resource : seatResources) {
        send_capabilities(resource->handle, capabilities);
    }
}

void SeatInterface::setHasKeyboard(bool has)
{
    if (d->keyboard.isNull() != has) {
        return;
    }
    if (has) {
        d->keyboard.reset(new KeyboardInterface(this));
        d->defaultKeyboardGrab.reset(new ForwardKeyboardGrab(this));
        d->defaultKeyboardGrab->setActive(true);
    } else {
        d->keyboard.reset();
        d->clearKeyboardGrab();
    }

    d->sendCapabilities();
    emit hasKeyboardChanged(d->keyboard);
}

void SeatInterface::setHasPointer(bool has)
{
    if (d->pointer.isNull() != has) {
        return;
    }
    if (has) {
        d->pointer.reset(new PointerInterface(this));
        d->defaultPointerGrab.reset(new ForwardPointerGrab(this));
        d->defaultPointerGrab->setActive(true);
    } else {
        d->pointer.reset();
        d->clearPointerGrab();
    }

    d->sendCapabilities();
    emit hasPointerChanged(d->pointer);
}

void SeatInterface::setHasTouch(bool has)
{
    if (d->touch.isNull() != has) {
        return;
    }
    if (has) {
        d->touch.reset(new TouchInterface(this));
        d->defaultTouchGrab.reset(new ForwardTouchGrab(this));
        d->defaultTouchGrab->setActive(true);
    } else {
        d->touch.reset();
        d->clearTouchGrab();
    }

    d->sendCapabilities();
    emit hasTouchChanged(d->touch);
}

void SeatInterface::setName(const QString &name)
{
    if (d->name == name) {
        return;
    }
    d->name = name;

    const auto seatResources = d->resourceMap();
    for (SeatInterfacePrivate::Resource *resource : seatResources) {
        if (resource->version() >= WL_SEAT_NAME_SINCE_VERSION) {
            d->send_name(resource->handle, d->name);
        }
    }

    emit nameChanged(d->name);
}

QString SeatInterface::name() const
{
    return d->name;
}

bool SeatInterface::hasPointer() const
{
    return d->pointer;
}

bool SeatInterface::hasKeyboard() const
{
    return d->keyboard;
}

bool SeatInterface::hasTouch() const
{
    return d->touch;
}

Display *SeatInterface::display() const
{
    return d->display;
}

PointerGrab *SeatInterface::pointerGrab() const
{
    return d->pointerGrab;
}

PointerGrab *SeatInterface::defaultPointerGrab() const
{
    return d->defaultPointerGrab.data();
}

KeyboardGrab *SeatInterface::keyboardGrab() const
{
    return d->keyboardGrab;
}

KeyboardGrab *SeatInterface::defaultKeyboardGrab() const
{
    return d->defaultKeyboardGrab.data();
}

TouchGrab *SeatInterface::touchGrab() const
{
    return d->touchGrab;
}

TouchGrab *SeatInterface::defaultTouchGrab() const
{
    return d->defaultTouchGrab.data();
}

void SeatInterfacePrivate::grabKeyboard(KeyboardGrab *grab)
{
    KeyboardGrab *oldGrab = keyboardGrab;
    keyboardGrab = grab;

    if (oldGrab) {
        oldGrab->cancel();
        oldGrab->deactivate();
    }
    keyboardGrab->activate();
}

void SeatInterfacePrivate::ungrabKeyboard(KeyboardGrab *grab)
{
    if (keyboardGrab != grab) {
        return;
    }
    keyboardGrab->deactivate();
    keyboardGrab = defaultKeyboardGrab.data();
    keyboardGrab->activate();
}

void SeatInterfacePrivate::grabTouch(TouchGrab *grab)
{
    TouchGrab *oldGrab = touchGrab;
    touchGrab = grab;

    if (oldGrab) {
        oldGrab->cancel();
        oldGrab->deactivate();
    }
    touchGrab->activate();
}

void SeatInterfacePrivate::ungrabTouch(TouchGrab *grab)
{
    if (touchGrab != grab) {
        return;
    }
    touchGrab->deactivate();
    touchGrab = defaultTouchGrab.data();
    touchGrab->activate();
}

void SeatInterfacePrivate::grabPointer(PointerGrab *grab)
{
    PointerGrab *oldGrab = pointerGrab;
    pointerGrab = grab;

    if (oldGrab) {
        oldGrab->cancel();
        oldGrab->deactivate();
    }
    pointerGrab->activate();
}

void SeatInterfacePrivate::ungrabPointer(PointerGrab *grab)
{
    if (pointerGrab != grab) {
        return;
    }
    pointerGrab->deactivate();
    pointerGrab = defaultPointerGrab.data();
    pointerGrab->activate();
}

void SeatInterfacePrivate::clearKeyboardGrab()
{
    keyboardGrab->cancel();
    keyboardGrab->deactivate();

    keyboardGrab = nullptr;
    defaultKeyboardGrab.reset();
}

void SeatInterfacePrivate::clearTouchGrab()
{
    touchGrab->cancel();
    touchGrab->deactivate();

    touchGrab = nullptr;
    defaultTouchGrab.reset();
}

void SeatInterfacePrivate::clearPointerGrab()
{
    pointerGrab->cancel();
    pointerGrab->deactivate();

    pointerGrab = nullptr;
    defaultPointerGrab.reset();
}

SeatInterface *SeatInterface::get(wl_resource *native)
{
    if (SeatInterfacePrivate *seatPrivate = resource_cast<SeatInterfacePrivate *>(native)) {
        return seatPrivate->q;
    }
    return nullptr;
}

QPointF SeatInterface::pointerPos() const
{
    return d->globalPointer.pos;
}

void SeatInterface::sendPointerMotionEvent(const QPointF &pos)
{
    if (d->globalPointer.pos == pos) {
        return;
    }
    d->globalPointer.pos = pos;

    SurfaceInterface *focusedSurface = focusedPointerSurface();
    if (!focusedSurface) {
        return;
    }
    if (isDragPointer()) {
        const auto *originSurface = dragSource()->origin();
        const bool proxyRemoteFocused = originSurface->dataProxy() && originSurface == focusedSurface;
        if (!proxyRemoteFocused) {
            // handled by DataDevice
            return;
        }
    }

    QPointF localPosition = focusedPointerSurfaceTransformation().map(pos);
    SurfaceInterface *effectiveFocusedSurface = focusedSurface->inputSurfaceAt(localPosition);
    if (!effectiveFocusedSurface) {
        effectiveFocusedSurface = focusedSurface;
    }
    if (SubSurfaceInterface *subsurface = effectiveFocusedSurface->subSurface()) {
        localPosition -= subsurface->mainPosition();
    }

    if (d->pointer->focusedSurface() != effectiveFocusedSurface) {
        d->pointerGrab->handleFocusChange(effectiveFocusedSurface, localPosition, d->display->nextSerial());
    }

    d->pointerGrab->handleMotion(localPosition);
    emit pointerPosChanged(pos);
}

quint32 SeatInterface::timestamp() const
{
    return d->timestamp;
}

void SeatInterface::setTimestamp(quint32 time)
{
    if (d->timestamp == time) {
        return;
    }
    d->timestamp = time;
    emit timestampChanged(time);
}

void SeatInterface::setDragTarget(SurfaceInterface *surface, const QPointF &globalPosition, const QMatrix4x4 &inputTransformation)
{
    if (surface == d->drag.surface) {
        // no change
        return;
    }
    const quint32 serial = d->display->nextSerial();
    if (d->drag.target) {
        d->drag.target->updateDragTarget(nullptr, serial);
    }


    // TODO: technically we can have mulitple data devices
    // and we should send the drag to all of them, but that seems overly complicated
    // in practice so far the only case for mulitple data devices is for clipboard overriding
    d->drag.target = nullptr;
    if (d->dataDevicesForSurface(surface).size() > 0) {
        d->drag.target = d->dataDevicesForSurface(surface).first();
    }

    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer) {
        sendPointerMotionEvent(globalPosition);
    } else if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Touch &&
               d->globalTouch.focus.firstTouchPos != globalPosition) {
        sendTouchMotionEvent(d->globalTouch.ids.first(), globalPosition);
    }
    if (d->drag.target) {
        d->drag.surface = surface;
        d->drag.transformation = inputTransformation;
        d->drag.target->updateDragTarget(surface, serial);
    } else {
        d->drag.surface = nullptr;
    }
    emit dragSurfaceChanged();
    return;
}

void SeatInterface::setDragTarget(SurfaceInterface *surface, const QMatrix4x4 &inputTransformation)
{
    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer) {
        setDragTarget(surface, pointerPos(), inputTransformation);
    } else {
        Q_ASSERT(d->drag.mode == SeatInterfacePrivate::Drag::Mode::Touch);
        setDragTarget(surface, d->globalTouch.focus.firstTouchPos, inputTransformation);
    }

}

SurfaceInterface *SeatInterface::focusedPointerSurface() const
{
    return d->globalPointer.focus.surface;
}

void SeatInterface::setFocusedPointerSurface(SurfaceInterface *surface, const QPointF &surfacePosition)
{
    QMatrix4x4 m;
    m.translate(-surfacePosition.x(), -surfacePosition.y());
    setFocusedPointerSurface(surface, m);
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.offset = surfacePosition;
    }
}

void SeatInterface::setFocusedPointerSurface(SurfaceInterface *surface, const QMatrix4x4 &transformation)
{
    if (!d->pointer) {
        qCWarning(KWAYLAND_SERVER) << "Can not set focused pointer surface without pointer capability";
        return;
    }
    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer) {
        // ignore
        return;
    }

    const quint32 serial = d->display->nextSerial();

    if (d->globalPointer.focus.surface) {
        disconnect(d->globalPointer.focus.destroyConnection);
    }
    d->globalPointer.focus = SeatInterfacePrivate::Pointer::Focus();
    d->globalPointer.focus.surface = surface;
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.destroyConnection = connect(surface, &QObject::destroyed, this, [this] {
            d->globalPointer.focus = SeatInterfacePrivate::Pointer::Focus();
        });
        d->globalPointer.focus.serial = serial;
        d->globalPointer.focus.transformation = transformation;
        d->globalPointer.focus.offset = QPointF();
    }

    if (surface) {
        QPointF localPosition = focusedPointerSurfaceTransformation().map(pointerPos());
        SurfaceInterface *effectiveFocusedSurface = surface->inputSurfaceAt(localPosition);
        if (!effectiveFocusedSurface) {
            effectiveFocusedSurface = surface;
        }
        if (SubSurfaceInterface *subsurface = effectiveFocusedSurface->subSurface()) {
            localPosition -= subsurface->mainPosition();
        }
        d->pointerGrab->handleFocusChange(effectiveFocusedSurface, localPosition, serial);
    } else {
        d->pointerGrab->handleFocusChange(nullptr, QPointF(), serial);
    }
}

void SeatInterface::setFocusedPointerSurfacePosition(const QPointF &surfacePosition)
{
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.offset = surfacePosition;
        d->globalPointer.focus.transformation = QMatrix4x4();
        d->globalPointer.focus.transformation.translate(-surfacePosition.x(), -surfacePosition.y());
    }
}

QPointF SeatInterface::focusedPointerSurfacePosition() const
{
    return d->globalPointer.focus.offset;
}

void SeatInterface::setFocusedPointerSurfaceTransformation(const QMatrix4x4 &transformation)
{
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.transformation = transformation;
    }
}

QMatrix4x4 SeatInterface::focusedPointerSurfaceTransformation() const
{
    return d->globalPointer.focus.transformation;
}

PointerInterface *SeatInterface::pointer() const
{
    return d->pointer.data();
}

namespace {
static quint32 qtToWaylandButton(Qt::MouseButton button)
{
#if HAVE_LINUX_INPUT_H
    static const QHash<Qt::MouseButton, quint32> s_buttons({
        {Qt::LeftButton, BTN_LEFT},
        {Qt::RightButton, BTN_RIGHT},
        {Qt::MiddleButton, BTN_MIDDLE},
        {Qt::ExtraButton1, BTN_BACK},    // note: QtWayland maps BTN_SIDE
        {Qt::ExtraButton2, BTN_FORWARD}, // note: QtWayland maps BTN_EXTRA
        {Qt::ExtraButton3, BTN_TASK},    // note: QtWayland maps BTN_FORWARD
        {Qt::ExtraButton4, BTN_EXTRA},   // note: QtWayland maps BTN_BACK
        {Qt::ExtraButton5, BTN_SIDE},    // note: QtWayland maps BTN_TASK
        {Qt::ExtraButton6, BTN_TASK + 1},
        {Qt::ExtraButton7, BTN_TASK + 2},
        {Qt::ExtraButton8, BTN_TASK + 3},
        {Qt::ExtraButton9, BTN_TASK + 4},
        {Qt::ExtraButton10, BTN_TASK + 5},
        {Qt::ExtraButton11, BTN_TASK + 6},
        {Qt::ExtraButton12, BTN_TASK + 7},
        {Qt::ExtraButton13, BTN_TASK + 8}
        // further mapping not possible, 0x120 is BTN_JOYSTICK
    });
    return s_buttons.value(button, 0);
#else
    return 0;
#endif
}
}

bool SeatInterface::isPointerButtonPressed(Qt::MouseButton button) const
{
    return isPointerButtonPressed(qtToWaylandButton(button));
}

bool SeatInterface::isPointerButtonPressed(quint32 button) const
{
    auto it = d->globalPointer.buttonStates.constFind(button);
    if (it == d->globalPointer.buttonStates.constEnd()) {
        return false;
    }
    return it.value() == SeatInterfacePrivate::Pointer::State::Pressed;
}

void SeatInterface::sendPointerAxisEvent(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source)
{
    Q_ASSERT(d->pointer);
    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    d->pointerGrab->handleAxis(orientation, delta, discreteDelta, source);
}

void SeatInterface::sendPointerPressEvent(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    sendPointerPressEvent(nativeButton);
}

void SeatInterface::sendPointerPressEvent(quint32 button)
{
    Q_ASSERT(d->pointer);
    const quint32 serial = d->display->nextSerial();
    d->updatePointerButtonSerial(button, serial);
    d->updatePointerButtonState(button, SeatInterfacePrivate::Pointer::State::Pressed);
    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    d->pointerGrab->handlePressed(button, serial);

    if (focusedPointerSurface() == focusedKeyboardSurface()) {
        if (d->keyboard) {
            d->keyboardGrab->handleFocusChange(d->pointer->focusedSurface(), serial);
        }
    }
}

void SeatInterface::sendPointerReleaseEvent(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    sendPointerReleaseEvent(nativeButton);
}

void SeatInterface::sendPointerReleaseEvent(quint32 button)
{
    Q_ASSERT(d->pointer);
    const quint32 serial = d->display->nextSerial();
    const quint32 currentButtonSerial = pointerButtonSerial(button);
    d->updatePointerButtonSerial(button, serial);
    d->updatePointerButtonState(button, SeatInterfacePrivate::Pointer::State::Released);
    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer) {
        if (d->drag.source->dragImplicitGrabSerial() != currentButtonSerial) {
            // not our drag button - ignore
            return;
        }
        d->endDrag(serial);
        return;
    }
    d->pointerGrab->handleReleased(button, serial);
}

void SeatInterface::sendPointerFrameEvent()
{
    Q_ASSERT(d->pointer);
    d->pointerGrab->handleFrame();
}

quint32 SeatInterface::pointerButtonSerial(Qt::MouseButton button) const
{
    return pointerButtonSerial(qtToWaylandButton(button));
}

quint32 SeatInterface::pointerButtonSerial(quint32 button) const
{
    auto it = d->globalPointer.buttonSerials.constFind(button);
    if (it == d->globalPointer.buttonSerials.constEnd()) {
        return 0;
    }
    return it.value();
}

void SeatInterface::relativePointerMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds)
{
    Q_ASSERT(d->pointer);

    auto relativePointer = RelativePointerV1Interface::get(pointer());
    if (relativePointer) {
        relativePointer->sendRelativeMotion(delta, deltaNonAccelerated, microseconds);
    }
}

void SeatInterface::startPointerSwipeGesture(quint32 fingerCount)
{
    Q_ASSERT(d->pointer);

    auto swipeGesture = PointerSwipeGestureV1Interface::get(pointer());
    if (swipeGesture) {
        swipeGesture->sendBegin(d->display->nextSerial(), fingerCount);
    }
}

void SeatInterface::updatePointerSwipeGesture(const QSizeF &delta)
{
    Q_ASSERT(d->pointer);

    auto swipeGesture = PointerSwipeGestureV1Interface::get(pointer());
    if (swipeGesture) {
        swipeGesture->sendUpdate(delta);
    }
}

void SeatInterface::endPointerSwipeGesture()
{
    Q_ASSERT(d->pointer);

    auto swipeGesture = PointerSwipeGestureV1Interface::get(pointer());
    if (swipeGesture) {
        swipeGesture->sendEnd(d->display->nextSerial());
    }
}

void SeatInterface::cancelPointerSwipeGesture()
{
    Q_ASSERT(d->pointer);

    auto swipeGesture = PointerSwipeGestureV1Interface::get(pointer());
    if (swipeGesture) {
        swipeGesture->sendCancel(d->display->nextSerial());
    }
}

void SeatInterface::startPointerPinchGesture(quint32 fingerCount)
{
    Q_ASSERT(d->pointer);

    auto pinchGesture = PointerPinchGestureV1Interface::get(pointer());
    if (pinchGesture) {
        pinchGesture->sendBegin(d->display->nextSerial(), fingerCount);
    }
}

void SeatInterface::updatePointerPinchGesture(const QSizeF &delta, qreal scale, qreal rotation)
{
    Q_ASSERT(d->pointer);

    auto pinchGesture = PointerPinchGestureV1Interface::get(pointer());
    if (pinchGesture) {
        pinchGesture->sendUpdate(delta, scale, rotation);
    }
}

void SeatInterface::endPointerPinchGesture()
{
    Q_ASSERT(d->pointer);

    auto pinchGesture = PointerPinchGestureV1Interface::get(pointer());
    if (pinchGesture) {
        pinchGesture->sendEnd(d->display->nextSerial());
    }
}

void SeatInterface::cancelPointerPinchGesture()
{
    Q_ASSERT(d->pointer);

    auto pinchGesture = PointerPinchGestureV1Interface::get(pointer());
    if (pinchGesture) {
        pinchGesture->sendCancel(d->display->nextSerial());
    }
}

SurfaceInterface *SeatInterface::focusedKeyboardSurface() const
{
    return d->globalKeyboard.focus.surface;
}

void SeatInterface::setFocusedKeyboardSurface(SurfaceInterface *surface)
{
    if (!d->keyboard) {
        qCWarning(KWAYLAND_SERVER) << "Can not set focused keyboard surface without keyboard capability";
        return;
    }

    const quint32 serial = d->display->nextSerial();

    if (d->globalKeyboard.focus.surface) {
        disconnect(d->globalKeyboard.focus.destroyConnection);
    }
    d->globalKeyboard.focus = SeatInterfacePrivate::Keyboard::Focus();
    d->globalKeyboard.focus.surface = surface;
    if (d->globalKeyboard.focus.surface) {
        d->globalKeyboard.focus.destroyConnection = connect(surface, &QObject::destroyed, this, [this] {
            d->globalKeyboard.focus = SeatInterfacePrivate::Keyboard::Focus();
        });
        d->globalKeyboard.focus.serial = serial;
    }

    d->keyboardGrab->handleFocusChange(surface, serial);

    // focused text input surface follows keyboard
    if (hasKeyboard()) {
        setFocusedTextInputSurface(surface);
    }
}

KeyboardInterface *SeatInterface::keyboard() const
{
    return d->keyboard.data();
}

void SeatInterface::sendKeyPressEvent(quint32 key)
{
    Q_ASSERT(d->keyboard);
    d->keyboardGrab->handlePressEvent(key);
}

void SeatInterface::sendKeyReleaseEvent(quint32 key)
{
    Q_ASSERT(d->keyboard);
    d->keyboardGrab->handleReleaseEvent(key);
}

void SeatInterface::sendKeyModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    Q_ASSERT(d->keyboard);
    d->keyboardGrab->handleModifiers(depressed, latched, locked, group);
}

void SeatInterface::sendTouchCancelEvent()
{
    d->touchGrab->handleCancel();

    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Touch) {
        // cancel the drag, don't drop. serial does not matter
        d->cancelDrag(0);
    }
    d->globalTouch.ids.clear();
}

SurfaceInterface *SeatInterface::focusedTouchSurface() const
{
    return d->globalTouch.focus.surface;
}

QPointF SeatInterface::focusedTouchSurfacePosition() const
{
    return d->globalTouch.focus.offset;
}

bool SeatInterface::isTouchSequence() const
{
    return !d->globalTouch.ids.isEmpty();
}

TouchInterface *SeatInterface::touch() const
{
    return d->touch.data();
}

void SeatInterface::setFocusedTouchSurface(SurfaceInterface *surface, const QPointF &surfacePosition)
{
    if (isTouchSequence()) {
        // changing surface not allowed during a touch sequence
        return;
    }
    Q_ASSERT(!isDragTouch());
    if (d->globalTouch.focus.surface) {
        disconnect(d->globalTouch.focus.destroyConnection);
    }
    d->globalTouch.focus = SeatInterfacePrivate::Touch::Focus();
    d->globalTouch.focus.surface = surface;
    d->globalTouch.focus.offset = surfacePosition;
    if (d->globalTouch.focus.surface) {
        d->globalTouch.focus.destroyConnection = connect(surface, &QObject::destroyed, this, [this] {
            if (isTouchSequence()) {
                // Surface destroyed during touch sequence - send a cancel
                d->touchGrab->handleCancel();
            }
            d->globalTouch.focus = SeatInterfacePrivate::Touch::Focus();
        });
    }
    d->touchGrab->handleFocusChange(surface);
}

void SeatInterface::setFocusedTouchSurfacePosition(const QPointF &surfacePosition)
{
    d->globalTouch.focus.offset = surfacePosition;
}

qint32 SeatInterface::sendTouchDownEvent(const QPointF &globalPosition)
{
    const qint32 id = d->globalTouch.ids.isEmpty() ? 0 : d->globalTouch.ids.lastKey() + 1;
    const qint32 serial = d->display->nextSerial();
    const auto pos = globalPosition - d->globalTouch.focus.offset;
    d->touchGrab->handleDown(id, serial, pos);

    if (id == 0) {
        d->globalTouch.focus.firstTouchPos = globalPosition;
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && hasPointer() && focusedTouchSurface()) {
        // TODO: Make the compositor emulate touch events.
        TouchInterfacePrivate *touchPrivate = TouchInterfacePrivate::get(d->touch.data());
        if (touchPrivate->touchesForClient(focusedTouchSurface()->client()).isEmpty()) {
            // If the client did not bind the touch interface fall back
            // to at least emulating touch through pointer events.
            d->pointerGrab->handleFocusChange(focusedTouchSurface(), pos, serial);
            d->pointerGrab->handleMotion(pos);
            d->pointerGrab->handleFrame();
        }
    }
#endif

    d->globalTouch.ids[id] = serial;
    return id;
}

void SeatInterface::sendTouchMotionEvent(qint32 id, const QPointF &globalPosition)
{
    Q_ASSERT(d->globalTouch.ids.contains(id));
    const auto pos = globalPosition - d->globalTouch.focus.offset;
    d->touchGrab->handleMotion(id, pos);

    if (id == 0) {
        d->globalTouch.focus.firstTouchPos = globalPosition;

        // TODO: Make the compositor emulate touch events.
        if (hasPointer() && focusedTouchSurface()) {
            TouchInterfacePrivate *touchPrivate = TouchInterfacePrivate::get(d->touch.data());
            if (touchPrivate->touchesForClient(focusedTouchSurface()->client()).isEmpty()) {
                // Client did not bind touch, fall back to emulating with pointer events.
                d->pointerGrab->handleMotion(pos);
                d->pointerGrab->handleFrame();
            }
        }
    }
    emit touchMoved(id, d->globalTouch.ids[id], globalPosition);
}

void SeatInterface::sendTouchUpEvent(qint32 id)
{
    Q_ASSERT(d->globalTouch.ids.contains(id));
    const qint32 serial = d->display->nextSerial();
    if (d->drag.mode == SeatInterfacePrivate::Drag::Mode::Touch &&
            d->drag.source->dragImplicitGrabSerial() == d->globalTouch.ids.value(id)) {
        // the implicitly grabbing touch point has been upped
        d->endDrag(serial);
    }
    d->touchGrab->handleUp(id, serial);

#if HAVE_LINUX_INPUT_H
    if (id == 0 && hasPointer() && focusedTouchSurface()) {
        // TODO: Make the compositor emulate touch events.
        TouchInterfacePrivate *touchPrivate = TouchInterfacePrivate::get(d->touch.data());
        if (touchPrivate->touchesForClient(focusedTouchSurface()->client()).isEmpty()) {
            // Client did not bind touch, fall back to emulating with pointer events.
            const quint32 serial = d->display->nextSerial();
            d->pointerGrab->handleReleased(BTN_LEFT, serial);
            d->pointerGrab->handleFrame();
        }
    }
#endif

    d->globalTouch.ids.remove(id);
}

void SeatInterface::sendTouchFrameEvent()
{
    d->touchGrab->handleFrame();
}

bool SeatInterface::hasImplicitTouchGrab(quint32 serial) const
{
    if (!d->globalTouch.focus.surface) {
        // origin surface has been destroyed
        return false;
    }
    return d->globalTouch.ids.key(serial, -1) != -1;
}

bool SeatInterface::isDrag() const
{
    return d->drag.mode != SeatInterfacePrivate::Drag::Mode::None;
}

bool SeatInterface::isDragPointer() const
{
    return d->drag.mode == SeatInterfacePrivate::Drag::Mode::Pointer;
}

bool SeatInterface::isDragTouch() const
{
    return d->drag.mode == SeatInterfacePrivate::Drag::Mode::Touch;
}

bool SeatInterface::hasImplicitPointerGrab(quint32 serial) const
{
    const auto &serials = d->globalPointer.buttonSerials;
    for (auto it = serials.constBegin(), end = serials.constEnd(); it != end; it++) {
        if (it.value() == serial) {
            return isPointerButtonPressed(it.key());
        }
    }
    return false;
}

QMatrix4x4 SeatInterface::dragSurfaceTransformation() const
{
    return d->drag.transformation;
}

SurfaceInterface *SeatInterface::dragSurface() const
{
    return d->drag.surface;
}

PointerInterface *SeatInterface::dragPointer() const
{
    if (d->drag.mode != SeatInterfacePrivate::Drag::Mode::Pointer) {
        return nullptr;
    }

    return d->drag.sourcePointer;
}

DataDeviceInterface *SeatInterface::dragSource() const
{
    return d->drag.source;
}

void SeatInterface::setFocusedTextInputSurface(SurfaceInterface *surface)
{
    const quint32 serial = d->display->nextSerial();

    if (d->focusedTextInputSurface) {
        disconnect(d->focusedSurfaceDestroyConnection);
    }

    if (d->focusedTextInputSurface != surface){
        d->textInputV2->d->sendLeave(serial, d->focusedTextInputSurface);
        d->textInputV3->d->sendLeave(d->focusedTextInputSurface);
        d->focusedTextInputSurface = surface;
        emit focusedTextInputSurfaceChanged();
    }

    if (d->focusedTextInputSurface) {
        d->focusedSurfaceDestroyConnection = connect(surface, &SurfaceInterface::aboutToBeDestroyed, this,
            [this] {
                setFocusedTextInputSurface(nullptr);
            }
        );
    }

    d->textInputV2->d->sendEnter(surface, serial);
    d->textInputV3->d->sendEnter(surface);
    // TODO: setFocusedSurface like in other interfaces
}

SurfaceInterface *SeatInterface::focusedTextInputSurface() const
{
    return d->focusedTextInputSurface;
}

TextInputV2Interface *SeatInterface::textInputV2() const
{
    return d->textInputV2;
}

TextInputV3Interface *SeatInterface::textInputV3() const
{
    return d->textInputV3;
}
AbstractDataSource *SeatInterface::selection() const
{
    return d->currentSelection;
}

void SeatInterface::setSelection(AbstractDataSource *selection)
{
    if (d->currentSelection == selection) {
        return;
    }

    if (d->currentSelection) {
        d->currentSelection->cancel();
        disconnect(d->currentSelection, nullptr, this, nullptr);
    }

    if (selection) {
        auto cleanup = [this]() {
            setSelection(nullptr);
        };
        connect(selection, &DataSourceInterface::aboutToBeDestroyed, this, cleanup);
    }

    d->currentSelection = selection;

    for (auto focussedSelection: qAsConst(d->globalKeyboard.focus.selections)) {
        if (selection) {
            focussedSelection->sendSelection(selection);
        } else {
            focussedSelection->sendClearSelection();
        }
    }

    for (auto control : qAsConst(d->dataControlDevices)) {
        if (selection) {
            control->sendSelection(selection);
        } else {
            control->sendClearSelection();
        }
    }

    emit selectionChanged(selection);
}

void SeatInterface::setPrimarySelection(AbstractDataSource *selection)
{
    if (d->currentPrimarySelection == selection) {
        return;
    }
    if (d->currentPrimarySelection) {
        d->currentPrimarySelection->cancel();
        disconnect(d->currentPrimarySelection, nullptr, this, nullptr);
    }

    if (selection) {
        auto cleanup = [this]() {
            setPrimarySelection(nullptr);
        };
        connect(selection, &DataSourceInterface::aboutToBeDestroyed, this, cleanup);
    }

    d->currentPrimarySelection = selection;

    for (auto focussedSelection: qAsConst(d->globalKeyboard.focus.primarySelections)) {
        if (selection) {
            focussedSelection->sendSelection(selection);
        } else {
            focussedSelection->sendClearSelection();
        }
    }

    emit primarySelectionChanged(selection);
}

ForwardPointerGrab::ForwardPointerGrab(SeatInterface *seat, QObject *parent)
    : PointerGrab(seat, parent)
{
}

void ForwardPointerGrab::cancel()
{
}

void ForwardPointerGrab::handleFocusChange(SurfaceInterface *surface, const QPointF &position, quint32 serial)
{
    seat()->pointer()->setFocusedSurface(surface, position, serial);
}

void ForwardPointerGrab::handlePressed(quint32 button, quint32 serial)
{
    seat()->pointer()->sendPressed(button, serial);
}

void ForwardPointerGrab::handleReleased(quint32 button, quint32 serial)
{
    seat()->pointer()->sendReleased(button, serial);
}

void ForwardPointerGrab::handleAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source)
{
    seat()->pointer()->sendAxis(orientation, delta, discreteDelta, source);
}

void ForwardPointerGrab::handleMotion(const QPointF &position)
{
    seat()->pointer()->sendMotion(position);
}

void ForwardPointerGrab::handleFrame()
{
    seat()->pointer()->sendFrame();
}

ForwardKeyboardGrab::ForwardKeyboardGrab(SeatInterface *seat, QObject *parent)
    : KeyboardGrab(seat, parent)
{
}

void ForwardKeyboardGrab::cancel()
{
}

void ForwardKeyboardGrab::handleFocusChange(SurfaceInterface *surface, quint32 serial)
{
    seat()->keyboard()->setFocusedSurface(surface, serial);
}

void ForwardKeyboardGrab::handlePressEvent(quint32 keyCode)
{
    seat()->keyboard()->sendPressed(keyCode);
}

void ForwardKeyboardGrab::handleReleaseEvent(quint32 keyCode)
{
    seat()->keyboard()->sendReleased(keyCode);
}

void ForwardKeyboardGrab::handleModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    seat()->keyboard()->sendModifiers(depressed, latched, locked, group);
}

ForwardTouchGrab::ForwardTouchGrab(SeatInterface *seat, QObject *parent)
    : TouchGrab(seat, parent)
{
}

void ForwardTouchGrab::cancel()
{
}

void ForwardTouchGrab::handleFocusChange(SurfaceInterface *surface)
{
    seat()->touch()->setFocusedSurface(surface);
}

void ForwardTouchGrab::handleDown(qint32 id, quint32 serial, const QPointF &localPos)
{
    seat()->touch()->sendDown(id, serial, localPos);
}

void ForwardTouchGrab::handleUp(qint32 id, quint32 serial)
{
    seat()->touch()->sendUp(id, serial);
}

void ForwardTouchGrab::handleFrame()
{
    seat()->touch()->sendFrame();
}

void ForwardTouchGrab::handleCancel()
{
    seat()->touch()->sendCancel();
}

void ForwardTouchGrab::handleMotion(qint32 id, const QPointF &localPos)
{
    seat()->touch()->sendMotion(id, localPos);
}

}
