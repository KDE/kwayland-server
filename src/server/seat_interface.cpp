/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "abstract_data_source.h"
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
#include "pointer_interface.h"
#include "pointer_interface_p.h"
#include "primaryselectiondevice_v1_interface.h"
#include "primaryselectionsource_v1_interface.h"
#include "surface_interface.h"
#include "textinput_v2_interface_p.h"
#include "textinput_v3_interface_p.h"
// Qt
#include <QFile>
// Wayland
#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif
// linux
#include <config-kwaylandserver.h>
#if HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

#include <functional>

namespace KWaylandServer
{

const quint32 SeatInterface::Private::s_version = 5;
const qint32 SeatInterface::Private::s_pointerVersion = 5;
const qint32 SeatInterface::Private::s_touchVersion = 5;
const qint32 SeatInterface::Private::s_keyboardVersion = 5;

SeatInterface::Private::Private(SeatInterface *q, Display *display)
    : Global::Private(display, &wl_seat_interface, s_version)
    , q(q)
{
    textInputV2 = new TextInputV2Interface(q);
    textInputV3 = new TextInputV3Interface(q);
}

#ifndef K_DOXYGEN
const struct wl_seat_interface SeatInterface::Private::s_interface = {
    getPointerCallback,
    getKeyboardCallback,
    getTouchCallback,
    releaseCallback
};
#endif

SeatInterface::SeatInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
    Q_D();
    connect(this, &SeatInterface::nameChanged, this,
        [d] {
            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                d->sendName(*it);
            }
        }
    );
    auto sendCapabilitiesAll = [d] {
        for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
            d->sendCapabilities(*it);
        }
    };
    connect(this, &SeatInterface::hasPointerChanged,  this, sendCapabilitiesAll);
    connect(this, &SeatInterface::hasKeyboardChanged, this, sendCapabilitiesAll);
    connect(this, &SeatInterface::hasTouchChanged,    this, sendCapabilitiesAll);
}

SeatInterface::~SeatInterface()
{
    Q_D();
    while (!d->resources.isEmpty()) {
        wl_resource_destroy(d->resources.takeLast());
    }
}

void SeatInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *r = wl_resource_create(client, &wl_seat_interface, qMin(s_version, version), id);
    if (!r) {
        wl_client_post_no_memory(client);
        return;
    }
    resources << r;

    wl_resource_set_implementation(r, &s_interface, this, unbind);

    sendCapabilities(r);
    sendName(r);
}

void SeatInterface::Private::unbind(wl_resource *r)
{
    cast(r)->resources.removeAll(r);
}

void SeatInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void SeatInterface::Private::updatePointerButtonSerial(quint32 button, quint32 serial)
{
    auto it = globalPointer.buttonSerials.find(button);
    if (it == globalPointer.buttonSerials.end()) {
        globalPointer.buttonSerials.insert(button, serial);
        return;
    }
    it.value() = serial;
}

void SeatInterface::Private::updatePointerButtonState(quint32 button, Pointer::State state)
{
    auto it = globalPointer.buttonStates.find(button);
    if (it == globalPointer.buttonStates.end()) {
        globalPointer.buttonStates.insert(button, state);
        return;
    }
    it.value() = state;
}

void SeatInterface::Private::sendName(wl_resource *r)
{
    if (wl_resource_get_version(r) < WL_SEAT_NAME_SINCE_VERSION) {
        return;
    }
    wl_seat_send_name(r, name.toUtf8().constData());
}

void SeatInterface::Private::sendCapabilities(wl_resource *r)
{
    uint32_t capabilities = 0;
    if (pointer) {
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (keyboard) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (touch) {
        capabilities |= WL_SEAT_CAPABILITY_TOUCH;
    }
    wl_seat_send_capabilities(r, capabilities);
}

quint32 SeatInterface::Private::nextSerial() const
{
    return display->nextSerial();
}

SeatInterface::Private *SeatInterface::Private::cast(wl_resource *r)
{
    return r ? reinterpret_cast<SeatInterface::Private*>(wl_resource_get_user_data(r)) : nullptr;
}

namespace {
template <typename T>
static
T *interfaceForSurface(SurfaceInterface *surface, const QVector<T*> &interfaces)
{
    if (!surface) {
        return nullptr;
    }

    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client() == surface->client()) {
            return (*it);
        }
    }
    return nullptr;
}

template <typename T>
static
QVector<T *> interfacesForSurface(SurfaceInterface *surface, const QVector<T*> &interfaces)
{
    QVector<T *> ret;
    if (!surface) {
        return ret;
    }

    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client() == surface->client() && (*it)->resource()) {
            ret << *it;
        }
    }
    return ret;
}

template <typename T>
static
bool forEachInterface(SurfaceInterface *surface, const QVector<T*> &interfaces, std::function<void (T*)> method)
{
    if (!surface) {
        return false;
    }
    bool calledAtLeastOne = false;
    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client() == surface->client() && (*it)->resource()) {
            method(*it);
            calledAtLeastOne = true;
        }
    }
    return calledAtLeastOne;
}

}

QVector<PointerInterface *> SeatInterface::Private::pointersForSurface(SurfaceInterface *surface) const
{
    return interfacesForSurface(surface, pointers);
}

QVector<TouchInterface *> SeatInterface::Private::touchsForSurface(SurfaceInterface *surface) const
{
    return interfacesForSurface(surface, touchs);
}

QVector<DataDeviceInterface *> SeatInterface::Private::dataDevicesForSurface(SurfaceInterface *surface) const
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

void SeatInterface::Private::registerDataDevice(DataDeviceInterface *dataDevice)
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
                drag.sourcePointer = interfaceForSurface(dragSurface, pointers);
                drag.transformation = globalPointer.focus.transformation;
            } else if (q->hasImplicitTouchGrab(dragSerial)) {
                drag.mode = Drag::Mode::Touch;
                drag.sourceTouch = interfaceForSurface(dragSurface, touchs);
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
            drag.sourcePointer = interfaceForSurface(originSurface, pointers);
            drag.destroyConnection = QObject::connect(dataDevice, &QObject::destroyed, q,
                [this] {
                    endDrag(display->nextSerial());
                }
            );
            if (dataDevice->dragSource()) {
                drag.dragSourceDestroyConnection = QObject::connect(dataDevice->dragSource(), &AbstractDataSource::aboutToBeDestroyed, q,
                    [this] {
                        const auto serial = display->nextSerial();
                        if (drag.target) {
                            drag.target->updateDragTarget(nullptr, serial);
                            drag.target = nullptr;
                        }
                        endDrag(serial);
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

void SeatInterface::Private::registerDataControlDevice(DataControlDeviceV1Interface *dataDevice)
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

void SeatInterface::Private::registerPrimarySelectionDevice(PrimarySelectionDeviceV1Interface *primarySelectionDevice)
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

void SeatInterface::Private::endDrag(quint32 serial)
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

void SeatInterface::Private::updateSelection(DataDeviceInterface *dataDevice)
{
    // if the update is from the focussed window we should inform the active client
    if (!(globalKeyboard.focus.surface && (*globalKeyboard.focus.surface->client() == dataDevice->client()))) {
        return;
    }
    q->setSelection(dataDevice->selection());
}

void SeatInterface::Private::updatePrimarySelection(PrimarySelectionDeviceV1Interface *primarySelectionDevice)
{
    // if the update is from the focussed window we should inform the active client
    if (!(globalKeyboard.focus.surface && (*globalKeyboard.focus.surface->client() == primarySelectionDevice->client()))) {
        return;
    }
    q->setPrimarySelection(primarySelectionDevice->selection());
}

void SeatInterface::setHasKeyboard(bool has)
{
    Q_D();
    if (d->keyboard.isNull() != has) {
        return;
    }
    if (has) {
        d->keyboard.reset(new KeyboardInterface(this));
    } else {
        d->keyboard.reset();
    }

    emit hasKeyboardChanged(d->keyboard);
}

void SeatInterface::setHasPointer(bool has)
{
    Q_D();
    if (d->pointer == has) {
        return;
    }
    d->pointer = has;
    emit hasPointerChanged(d->pointer);
}

void SeatInterface::setHasTouch(bool has)
{
    Q_D();
    if (d->touch == has) {
        return;
    }
    d->touch = has;
    emit hasTouchChanged(d->touch);
}

void SeatInterface::setName(const QString &name)
{
    Q_D();
    if (d->name == name) {
        return;
    }
    d->name = name;
    emit nameChanged(d->name);
}

void SeatInterface::Private::getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->getPointer(client, resource, id);
}

void SeatInterface::Private::getPointer(wl_client *client, wl_resource *resource, uint32_t id)
{
    // TODO: only create if seat has pointer?
    PointerInterface *pointer = new PointerInterface(q, resource);
    auto clientConnection = display->getConnection(client);
    pointer->create(clientConnection, qMin(wl_resource_get_version(resource), s_pointerVersion), id);
    if (!pointer->resource()) {
        wl_resource_post_no_memory(resource);
        delete pointer;
        return;
    }
    pointers << pointer;
    if (globalPointer.focus.surface && globalPointer.focus.surface->client() == clientConnection) {
        // this is a pointer for the currently focused pointer surface
        globalPointer.focus.pointers << pointer;
        pointer->setFocusedSurface(globalPointer.focus.surface, globalPointer.focus.serial);
        pointer->d_func()->sendFrame();
        if (globalPointer.focus.pointers.count() == 1) {
            // got a new pointer
            emit q->focusedPointerChanged(pointer);
        }
    }
    QObject::connect(pointer, &QObject::destroyed, q,
        [pointer,this] {
            pointers.removeAt(pointers.indexOf(pointer));
            if (globalPointer.focus.pointers.removeOne(pointer)) {
                if (globalPointer.focus.pointers.isEmpty()) {
                    emit q->focusedPointerChanged(nullptr);
                }
            }
        }
    );
    emit q->pointerCreated(pointer);
}

void SeatInterface::Private::getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->getKeyboard(client, resource, id);
}

void SeatInterface::Private::getKeyboard(wl_client *client, wl_resource *resource, uint32_t id)
{
    if (!keyboard) {
        return;
    }

    keyboard->d->add(client, id, wl_resource_get_version(resource));
    emit q->keyboardCreated(keyboard.data());
}

void SeatInterface::Private::grabKeyboard(wl_client *client, wl_resource *resource, uint32_t id)
{
    if (!keyboard) {
        qCWarning(KWAYLAND_SERVER) << "Trying to grab keyboard on seat without keyboard capability.";
        wl_resource_post_no_memory(resource);
        return;
    }
    if (!grabKeyboardInterface) {
        grabKeyboardInterface.reset(new KeyboardInterface(q));
    }
    grabKeyboardInterface->d->add(client, id, wl_resource_get_version(resource));
    grabKeyboardInterface->d->keyboardGrabbed(client);
}

void SeatInterface::Private::getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->getTouch(client, resource, id);
}

void SeatInterface::Private::getTouch(wl_client *client, wl_resource *resource, uint32_t id)
{
    // TODO: only create if seat has touch?
    TouchInterface *touch = new TouchInterface(q, resource);
    auto clientConnection = display->getConnection(client);
    touch->create(clientConnection, qMin(wl_resource_get_version(resource), s_touchVersion), id);
    if (!touch->resource()) {
        wl_resource_post_no_memory(resource);
        delete touch;
        return;
    }
    touchs << touch;
    if (globalTouch.focus.surface && globalTouch.focus.surface->client() == clientConnection) {
        // this is a touch for the currently focused touch surface
        globalTouch.focus.touchs << touch;
        if (!globalTouch.ids.isEmpty()) {
            // TODO: send out all the points
        }
    }
    QObject::connect(touch, &QObject::destroyed, q,
        [touch,this] {
            touchs.removeAt(touchs.indexOf(touch));
            globalTouch.focus.touchs.removeOne(touch);
        }
    );
    emit q->touchCreated(touch);
}

QString SeatInterface::name() const
{
    Q_D();
    return d->name;
}

bool SeatInterface::hasPointer() const
{
    Q_D();
    return d->pointer;
}

bool SeatInterface::hasKeyboard() const
{
    Q_D();
    return d->keyboard;
}

bool SeatInterface::hasTouch() const
{
    Q_D();
    return d->touch;
}

SeatInterface *SeatInterface::get(wl_resource *native)
{
    return Private::get(native);
}

SeatInterface::Private *SeatInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QPointF SeatInterface::pointerPos() const
{
    Q_D();
    return d->globalPointer.pos;
}

void SeatInterface::setPointerPos(const QPointF &pos)
{
    Q_D();
    if (d->globalPointer.pos == pos) {
        return;
    }
    d->globalPointer.pos = pos;
    emit pointerPosChanged(pos);
}

quint32 SeatInterface::timestamp() const
{
    Q_D();
    return d->timestamp;
}

void SeatInterface::setTimestamp(quint32 time)
{
    Q_D();
    if (d->timestamp == time) {
        return;
    }
    d->timestamp = time;
    emit timestampChanged(time);
}

void SeatInterface::setDragTarget(SurfaceInterface *surface, const QPointF &globalPosition, const QMatrix4x4 &inputTransformation)
{
    Q_D();
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

    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        setPointerPos(globalPosition);
    } else if (d->drag.mode == Private::Drag::Mode::Touch &&
               d->globalTouch.focus.firstTouchPos != globalPosition) {
        touchMove(d->globalTouch.ids.first(), globalPosition);
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
    Q_D();
    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        setDragTarget(surface, pointerPos(), inputTransformation);
    } else {
        Q_ASSERT(d->drag.mode == Private::Drag::Mode::Touch);
        setDragTarget(surface, d->globalTouch.focus.firstTouchPos, inputTransformation);
    }

}

SurfaceInterface *SeatInterface::focusedPointerSurface() const
{
    Q_D();
    return d->globalPointer.focus.surface;
}

void SeatInterface::setFocusedPointerSurface(SurfaceInterface *surface, const QPointF &surfacePosition)
{
    QMatrix4x4 m;
    m.translate(-surfacePosition.x(), -surfacePosition.y());
    setFocusedPointerSurface(surface, m);
    Q_D();
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.offset = surfacePosition;
    }
}

void SeatInterface::setFocusedPointerSurface(SurfaceInterface *surface, const QMatrix4x4 &transformation)
{
    Q_D();
    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    const quint32 serial = d->display->nextSerial();
    QSet<PointerInterface *> framePointers;
    for (auto it = d->globalPointer.focus.pointers.constBegin(), end = d->globalPointer.focus.pointers.constEnd(); it != end; ++it) {
        (*it)->setFocusedSurface(nullptr, serial);
        framePointers << *it;
    }
    if (d->globalPointer.focus.surface) {
        disconnect(d->globalPointer.focus.destroyConnection);
    }
    d->globalPointer.focus = Private::Pointer::Focus();
    d->globalPointer.focus.surface = surface;
    auto p = d->pointersForSurface(surface);
    d->globalPointer.focus.pointers = p;
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.destroyConnection = connect(surface, &QObject::destroyed, this,
            [this] {
                Q_D();
                d->globalPointer.focus = Private::Pointer::Focus();
                emit focusedPointerChanged(nullptr);
            }
        );
        d->globalPointer.focus.offset = QPointF();
        d->globalPointer.focus.transformation = transformation;
        d->globalPointer.focus.serial = serial;
    }
    if (p.isEmpty()) {
        emit focusedPointerChanged(nullptr);
        for (auto p : qAsConst(framePointers))
        {
            p->d_func()->sendFrame();
        }
        return;
    }
    // TODO: signal with all pointers
    emit focusedPointerChanged(p.first());
    for (auto it = p.constBegin(), end = p.constEnd(); it != end; ++it) {
        (*it)->setFocusedSurface(surface, serial);
        framePointers << *it;
    }
    for (auto p : qAsConst(framePointers))
    {
        p->d_func()->sendFrame();
    }
}

PointerInterface *SeatInterface::focusedPointer() const
{
    Q_D();
    if (d->globalPointer.focus.pointers.isEmpty()) {
        return nullptr;
    }
    return d->globalPointer.focus.pointers.first();
}

void SeatInterface::setFocusedPointerSurfacePosition(const QPointF &surfacePosition)
{
    Q_D();
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.offset = surfacePosition;
        d->globalPointer.focus.transformation = QMatrix4x4();
        d->globalPointer.focus.transformation.translate(-surfacePosition.x(), -surfacePosition.y());
    }
}

QPointF SeatInterface::focusedPointerSurfacePosition() const
{
    Q_D();
    return d->globalPointer.focus.offset;
}

void SeatInterface::setFocusedPointerSurfaceTransformation(const QMatrix4x4 &transformation)
{
    Q_D();
    if (d->globalPointer.focus.surface) {
        d->globalPointer.focus.transformation = transformation;
    }
}

QMatrix4x4 SeatInterface::focusedPointerSurfaceTransformation() const
{
    Q_D();
    return d->globalPointer.focus.transformation;
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
    Q_D();
    auto it = d->globalPointer.buttonStates.constFind(button);
    if (it == d->globalPointer.buttonStates.constEnd()) {
        return false;
    }
    return it.value() == Private::Pointer::State::Pressed ? true : false;
}

void SeatInterface::pointerAxisV5(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source)
{
    Q_D();
    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d->globalPointer.focus.surface) {
        for (auto it = d->globalPointer.focus.pointers.constBegin(), end = d->globalPointer.focus.pointers.constEnd(); it != end; ++it) {
            (*it)->axis(orientation, delta, discreteDelta, source);
        }
    }
}

void SeatInterface::pointerAxis(Qt::Orientation orientation, quint32 delta)
{
    Q_D();
    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d->globalPointer.focus.surface) {
        for (auto it = d->globalPointer.focus.pointers.constBegin(), end = d->globalPointer.focus.pointers.constEnd(); it != end; ++it) {
            (*it)->axis(orientation, delta);
        }
    }
}

void SeatInterface::pointerButtonPressed(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonPressed(nativeButton);
}

void SeatInterface::pointerButtonPressed(quint32 button)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    d->updatePointerButtonSerial(button, serial);
    d->updatePointerButtonState(button, Private::Pointer::State::Pressed);
    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (auto *focusSurface = d->globalPointer.focus.surface) {
        for (auto it = d->globalPointer.focus.pointers.constBegin(), end = d->globalPointer.focus.pointers.constEnd(); it != end; ++it) {
            (*it)->buttonPressed(button, serial);
        }
        if (focusSurface == d->globalKeyboard.focus.surface) {
            // update the focused child surface
            auto p = focusedPointer();
            if (p && d->keyboard) {
                d->keyboard->d->focusChildSurface(p->d_func()->focusedChildSurface, serial);
            }
        }
    }
}

void SeatInterface::pointerButtonReleased(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonReleased(nativeButton);
}

void SeatInterface::pointerButtonReleased(quint32 button)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    const quint32 currentButtonSerial = pointerButtonSerial(button);
    d->updatePointerButtonSerial(button, serial);
    d->updatePointerButtonState(button, Private::Pointer::State::Released);
    if (d->drag.mode == Private::Drag::Mode::Pointer) {
        if (d->drag.source->dragImplicitGrabSerial() != currentButtonSerial) {
            // not our drag button - ignore
            return;
        }
        d->endDrag(serial);
        return;
    }
    if (d->globalPointer.focus.surface) {
        for (auto it = d->globalPointer.focus.pointers.constBegin(), end = d->globalPointer.focus.pointers.constEnd(); it != end; ++it) {
            (*it)->buttonReleased(button, serial);
        }
    }
}

quint32 SeatInterface::pointerButtonSerial(Qt::MouseButton button) const
{
    return pointerButtonSerial(qtToWaylandButton(button));
}

quint32 SeatInterface::pointerButtonSerial(quint32 button) const
{
    Q_D();
    auto it = d->globalPointer.buttonSerials.constFind(button);
    if (it == d->globalPointer.buttonSerials.constEnd()) {
        return 0;
    }
    return it.value();
}

void SeatInterface::relativePointerMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds)
{
    Q_D();
    if (d->globalPointer.focus.surface) {
        for (auto it = d->globalPointer.focus.pointers.constBegin(), end = d->globalPointer.focus.pointers.constEnd(); it != end; ++it) {
            (*it)->relativeMotion(delta, deltaNonAccelerated, microseconds);
        }
    }
}

void SeatInterface::startPointerSwipeGesture(quint32 fingerCount)
{
    Q_D();
    if (!d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    d->globalPointer.gestureSurface = QPointer<SurfaceInterface>(d->globalPointer.focus.surface);
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d->display->nextSerial();
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [serial, fingerCount] (PointerInterface *p) {
            p->d_func()->startSwipeGesture(serial, fingerCount);
        }
    );
}

void SeatInterface::updatePointerSwipeGesture(const QSizeF &delta)
{
    Q_D();
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [delta] (PointerInterface *p) {
            p->d_func()->updateSwipeGesture(delta);
        }
    );
}

void SeatInterface::endPointerSwipeGesture()
{
    Q_D();
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d->display->nextSerial();
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [serial] (PointerInterface *p) {
            p->d_func()->endSwipeGesture(serial);
        }
    );
    d->globalPointer.gestureSurface.clear();
}

void SeatInterface::cancelPointerSwipeGesture()
{
    Q_D();
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d->display->nextSerial();
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [serial] (PointerInterface *p) {
            p->d_func()->cancelSwipeGesture(serial);
        }
    );
    d->globalPointer.gestureSurface.clear();
}

void SeatInterface::startPointerPinchGesture(quint32 fingerCount)
{
    Q_D();
    if (!d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    d->globalPointer.gestureSurface = QPointer<SurfaceInterface>(d->globalPointer.focus.surface);
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d->display->nextSerial();
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [serial, fingerCount] (PointerInterface *p) {
            p->d_func()->startPinchGesture(serial, fingerCount);
        }
    );
}

void SeatInterface::updatePointerPinchGesture(const QSizeF &delta, qreal scale, qreal rotation)
{
    Q_D();
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [delta, scale, rotation] (PointerInterface *p) {
            p->d_func()->updatePinchGesture(delta, scale, rotation);
        }
    );
}

void SeatInterface::endPointerPinchGesture()
{
    Q_D();
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d->display->nextSerial();
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [serial] (PointerInterface *p) {
            p->d_func()->endPinchGesture(serial);
        }
    );
    d->globalPointer.gestureSurface.clear();
}

void SeatInterface::cancelPointerPinchGesture()
{
    Q_D();
    if (d->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d->display->nextSerial();
    forEachInterface<PointerInterface>(d->globalPointer.gestureSurface.data(), d->pointers,
        [serial] (PointerInterface *p) {
            p->d_func()->cancelPinchGesture(serial);
        }
    );
    d->globalPointer.gestureSurface.clear();
}

SurfaceInterface *SeatInterface::focusedKeyboardSurface() const
{
    Q_D();
    return d->globalKeyboard.focus.surface;
}

void SeatInterface::setFocusedKeyboardSurface(SurfaceInterface *surface)
{
    Q_D();
    if (!d->keyboard) {
        qCWarning(KWAYLAND_SERVER) << "Can not set focused keyboard surface without keyboard capability";
        return;
    }

    const quint32 serial = d->display->nextSerial();

    if (d->globalKeyboard.focus.surface) {
        disconnect(d->globalKeyboard.focus.destroyConnection);
    }
    d->globalKeyboard.focus = Private::Keyboard::Focus();
    d->globalKeyboard.focus.surface = surface;
    if (d->globalKeyboard.focus.surface) {
        d->globalKeyboard.focus.destroyConnection = connect(surface, &QObject::destroyed, this,
            [this] {
                Q_D();
                d->globalKeyboard.focus = Private::Keyboard::Focus();
            }
        );
        d->globalKeyboard.focus.serial = serial;
        // selection?
        const QVector<DataDeviceInterface *> dataDevices = d->dataDevicesForSurface(surface);
        d->globalKeyboard.focus.selections = dataDevices;
        for (auto dataDevice : dataDevices) {
            if (d->currentSelection) {
                dataDevice->sendSelection(d->currentSelection);
            } else {
                dataDevice->sendClearSelection();
            }
        }
        // primary selection
        QVector<PrimarySelectionDeviceV1Interface *> primarySelectionDevices;
        for (auto it = d->primarySelectionDevices.constBegin(); it != d->primarySelectionDevices.constEnd(); ++it) {
            if ((*it)->client() == *surface->client()) {
                primarySelectionDevices << *it;
            }
        }

        d->globalKeyboard.focus.primarySelections = primarySelectionDevices;
        for (auto primaryDataDevice : primarySelectionDevices) {
            if (d->currentSelection) {
                primaryDataDevice->sendSelection(d->currentPrimarySelection);
            } else {
                primaryDataDevice->sendClearSelection();
            }
        }
    }

    d->keyboard->setFocusedSurface(surface, serial);

    // focused text input surface follows keyboard
    if (hasKeyboard()) {
        setFocusedTextInputSurface(surface);
    }
}

KeyboardInterface *SeatInterface::keyboard() const
{
    Q_D();
    return d->keyboard.data();
}

KeyboardInterface *SeatInterface::grabbedKeyboard() const
{
    Q_D();
    return d->grabKeyboardInterface.data();
}

void SeatInterface::cancelTouchSequence()
{
    Q_D();
    for (auto it = d->globalTouch.focus.touchs.constBegin(), end = d->globalTouch.focus.touchs.constEnd(); it != end; ++it) {
        (*it)->cancel();
    }
    if (d->drag.mode == Private::Drag::Mode::Touch) {
        // cancel the drag, don't drop.
        if (d->drag.target) {
            // remove the current target
            d->drag.target->updateDragTarget(nullptr, 0);
            d->drag.target = nullptr;
        }
        // and end the drag for the source, serial does not matter
        d->endDrag(0);
    }
    d->globalTouch.ids.clear();
}

TouchInterface *SeatInterface::focusedTouch() const
{
    Q_D();
    if (d->globalTouch.focus.touchs.isEmpty()) {
        return nullptr;
    }
    return d->globalTouch.focus.touchs.first();
}

SurfaceInterface *SeatInterface::focusedTouchSurface() const
{
    Q_D();
    return d->globalTouch.focus.surface;
}

QPointF SeatInterface::focusedTouchSurfacePosition() const
{
    Q_D();
    return d->globalTouch.focus.offset;
}

bool SeatInterface::isTouchSequence() const
{
    Q_D();
    return !d->globalTouch.ids.isEmpty();
}

void SeatInterface::setFocusedTouchSurface(SurfaceInterface *surface, const QPointF &surfacePosition)
{
    if (isTouchSequence()) {
        // changing surface not allowed during a touch sequence
        return;
    }
    Q_ASSERT(!isDragTouch());
    Q_D();
    if (d->globalTouch.focus.surface) {
        disconnect(d->globalTouch.focus.destroyConnection);
    }
    d->globalTouch.focus = Private::Touch::Focus();
    d->globalTouch.focus.surface = surface;
    d->globalTouch.focus.offset = surfacePosition;
    d->globalTouch.focus.touchs = d->touchsForSurface(surface);
    if (d->globalTouch.focus.surface) {
        d->globalTouch.focus.destroyConnection = connect(surface, &QObject::destroyed, this,
            [this] {
                Q_D();
                if (isTouchSequence()) {
                    // Surface destroyed during touch sequence - send a cancel
                    for (auto it = d->globalTouch.focus.touchs.constBegin(), end = d->globalTouch.focus.touchs.constEnd(); it != end; ++it) {
                        (*it)->cancel();
                    }
                }
                d->globalTouch.focus = Private::Touch::Focus();
            }
        );
    }
}

void SeatInterface::setFocusedTouchSurfacePosition(const QPointF &surfacePosition)
{
    Q_D();
    d->globalTouch.focus.offset = surfacePosition;
}

qint32 SeatInterface::touchDown(const QPointF &globalPosition)
{
    Q_D();
    const qint32 id = d->globalTouch.ids.isEmpty() ? 0 : d->globalTouch.ids.lastKey() + 1;
    const qint32 serial = display()->nextSerial();
    const auto pos = globalPosition - d->globalTouch.focus.offset;
    for (auto it = d->globalTouch.focus.touchs.constBegin(), end = d->globalTouch.focus.touchs.constEnd(); it != end; ++it) {
        (*it)->down(id, serial, pos);
    }

    if (id == 0) {
        d->globalTouch.focus.firstTouchPos = globalPosition;
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && d->globalTouch.focus.touchs.isEmpty()) {
        // If the client did not bind the touch interface fall back
        // to at least emulating touch through pointer events.
        forEachInterface<PointerInterface>(focusedTouchSurface(), d->pointers,
            [this, pos, serial] (PointerInterface *p) {
                wl_pointer_send_enter(p->resource(), serial,
                                focusedTouchSurface()->resource(),
                                wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
                wl_pointer_send_motion(p->resource(), timestamp(),
                                        wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));

                wl_pointer_send_button(p->resource(), serial, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
                p->d_func()->sendFrame();
            }
        );
    }
#endif

    d->globalTouch.ids[id] = serial;
    return id;
}

void SeatInterface::touchMove(qint32 id, const QPointF &globalPosition)
{
    Q_D();
    Q_ASSERT(d->globalTouch.ids.contains(id));
    const auto pos = globalPosition - d->globalTouch.focus.offset;
    for (auto it = d->globalTouch.focus.touchs.constBegin(), end = d->globalTouch.focus.touchs.constEnd(); it != end; ++it) {
        (*it)->move(id, pos);
    }

    if (id == 0) {
        d->globalTouch.focus.firstTouchPos = globalPosition;
    }

    if (id == 0 && d->globalTouch.focus.touchs.isEmpty()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        forEachInterface<PointerInterface>(focusedTouchSurface(), d->pointers,
            [this, pos] (PointerInterface *p) {
                wl_pointer_send_motion(p->resource(), timestamp(),
                                       wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
                p->d_func()->sendFrame();
            }
        );
    }
    emit touchMoved(id, d->globalTouch.ids[id], globalPosition);
}

void SeatInterface::touchUp(qint32 id)
{
    Q_D();
    Q_ASSERT(d->globalTouch.ids.contains(id));
    const qint32 serial = display()->nextSerial();
    if (d->drag.mode == Private::Drag::Mode::Touch &&
            d->drag.source->dragImplicitGrabSerial() == d->globalTouch.ids.value(id)) {
        // the implicitly grabbing touch point has been upped
        d->endDrag(serial);
    }
    for (auto it = d->globalTouch.focus.touchs.constBegin(), end = d->globalTouch.focus.touchs.constEnd(); it != end; ++it) {
        (*it)->up(id, serial);
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && d->globalTouch.focus.touchs.isEmpty()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        const quint32 serial = display()->nextSerial();
        forEachInterface<PointerInterface>(focusedTouchSurface(), d->pointers,
            [this, serial] (PointerInterface *p) {
                wl_pointer_send_button(p->resource(), serial, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
                p->d_func()->sendFrame();
            }
        );
    }
#endif

    d->globalTouch.ids.remove(id);
}

void SeatInterface::touchFrame()
{
    Q_D();
    for (auto it = d->globalTouch.focus.touchs.constBegin(), end = d->globalTouch.focus.touchs.constEnd(); it != end; ++it) {
        (*it)->frame();
    }
}

bool SeatInterface::hasImplicitTouchGrab(quint32 serial) const
{
    Q_D();
    if (!d->globalTouch.focus.surface) {
        // origin surface has been destroyed
        return false;
    }
    return d->globalTouch.ids.key(serial, -1) != -1;
}

bool SeatInterface::isDrag() const
{
    Q_D();
    return d->drag.mode != Private::Drag::Mode::None;
}

bool SeatInterface::isDragPointer() const
{
    Q_D();
    return d->drag.mode == Private::Drag::Mode::Pointer;
}

bool SeatInterface::isDragTouch() const
{
    Q_D();
    return d->drag.mode == Private::Drag::Mode::Touch;
}

bool SeatInterface::hasImplicitPointerGrab(quint32 serial) const
{
    Q_D();
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
    Q_D();
    return d->drag.transformation;
}

SurfaceInterface *SeatInterface::dragSurface() const
{
    Q_D();
    return d->drag.surface;
}

PointerInterface *SeatInterface::dragPointer() const
{
    Q_D();
    if (d->drag.mode != Private::Drag::Mode::Pointer) {
        return nullptr;
    }

    return d->drag.sourcePointer;
}

DataDeviceInterface *SeatInterface::dragSource() const
{
    Q_D();
    return d->drag.source;
}

void SeatInterface::setFocusedTextInputSurface(SurfaceInterface *surface)
{
    Q_D();
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
    Q_D();
    return d->focusedTextInputSurface;
}

TextInputV2Interface *SeatInterface::textInputV2() const
{
    Q_D();
    return d->textInputV2;
}

TextInputV3Interface *SeatInterface::textInputV3() const
{
    Q_D();
    return d->textInputV3;
}
AbstractDataSource *SeatInterface::selection() const
{
    Q_D();
    return d->currentSelection;
}

void SeatInterface::setSelection(AbstractDataSource *selection)
{
    Q_D();
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
    Q_D();
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

}
