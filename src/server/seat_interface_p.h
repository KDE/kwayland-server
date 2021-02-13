/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_SEAT_INTERFACE_P_H
#define WAYLAND_SERVER_SEAT_INTERFACE_P_H
// KWayland
#include "seat_interface.h"
#include "keyboardgrab.h"
#include "pointergrab.h"
#include "touchgrab.h"
// Qt
#include <QHash>
#include <QMap>
#include <QPointer>
#include <QVector>

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{

class AbstractDataSource;
class DataDeviceInterface;
class DataSourceInterface;
class DataControlDeviceV1Interface;
class TextInputV2Interface;
class TextInputV3Interface;
class PrimarySelectionDeviceV1Interface;

class ForwardPointerGrab final : public PointerGrab
{
    Q_OBJECT

public:
    explicit ForwardPointerGrab(SeatInterface *seat, QObject *parent = nullptr);

    void cancel() override;

    void handleFocusChange(SurfaceInterface *surface, const QPointF &position, quint32 serial) override;
    void handlePressed(quint32 button, quint32 serial) override;
    void handleReleased(quint32 button, quint32 serial) override;
    void handleAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source) override;
    void handleMotion(const QPointF &position) override;
    void handleFrame() override;
};

class ForwardKeyboardGrab final : public KeyboardGrab
{
    Q_OBJECT

public:
    explicit ForwardKeyboardGrab(SeatInterface *seat, QObject *parent = nullptr);

    void cancel() override;

    void handleFocusChange(SurfaceInterface *surface, quint32 serial) override;
    void handlePressEvent(quint32 keyCode) override;
    void handleReleaseEvent(quint32 keyCode) override;
    void handleModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group) override;
};

class ForwardTouchGrab final : public TouchGrab
{
    Q_OBJECT

public:
    explicit ForwardTouchGrab(SeatInterface *seat, QObject *parent = nullptr);

    void cancel() override;

    void handleFocusChange(SurfaceInterface *surface) override;
    void handleDown(qint32 id, quint32 serial, const QPointF &localPos) override;
    void handleUp(qint32 id, quint32 serial) override;
    void handleFrame() override;
    void handleCancel() override;
    void handleMotion(qint32 id, const QPointF &localPos) override;
};

class SeatInterfacePrivate : public QtWaylandServer::wl_seat
{
public:
    static SeatInterfacePrivate *get(SeatInterface *seat);
    SeatInterfacePrivate(SeatInterface *q, Display *display);

    void sendCapabilities();
    QVector<DataDeviceInterface *> dataDevicesForSurface(SurfaceInterface *surface) const;
    void registerPrimarySelectionDevice(PrimarySelectionDeviceV1Interface *primarySelectionDevice);
    void registerDataDevice(DataDeviceInterface *dataDevice);
    void registerDataControlDevice(DataControlDeviceV1Interface *dataDevice);
    void endDrag(quint32 serial);
    void cancelDrag(quint32 serial);
    quint32 nextSerial() const;

    void grabKeyboard(KeyboardGrab *grab);
    void ungrabKeyboard(KeyboardGrab *grab);
    void grabTouch(TouchGrab *grab);
    void ungrabTouch(TouchGrab *grab);
    void grabPointer(PointerGrab *grab);
    void ungrabPointer(PointerGrab *grab);
    void clearTouchGrab();
    void clearKeyboardGrab();
    void clearPointerGrab();

    SeatInterface *q;
    Display *display;
    QString name;
    QList<wl_resource*> resources;
    quint32 timestamp = 0;
    QScopedPointer<KeyboardInterface> keyboard;
    QScopedPointer<PointerInterface> pointer;
    QScopedPointer<TouchInterface> touch;
    QVector<DataDeviceInterface*> dataDevices;
    QVector<PrimarySelectionDeviceV1Interface*> primarySelectionDevices;
    QVector<DataControlDeviceV1Interface*> dataControlDevices;
    PointerGrab *pointerGrab = nullptr;
    QScopedPointer<PointerGrab> defaultPointerGrab;
    KeyboardGrab *keyboardGrab = nullptr;
    QScopedPointer<KeyboardGrab> defaultKeyboardGrab;
    TouchGrab *touchGrab = nullptr;
    QScopedPointer<TouchGrab> defaultTouchGrab;

    // TextInput v2
    QPointer<TextInputV2Interface> textInputV2;
    QPointer<TextInputV3Interface> textInputV3;

    SurfaceInterface *focusedTextInputSurface = nullptr;
    QMetaObject::Connection focusedSurfaceDestroyConnection;

    // the last thing copied into the clipboard content
    AbstractDataSource *currentSelection = nullptr;
    AbstractDataSource *currentPrimarySelection = nullptr;

    // Pointer related members
    struct Pointer {
        enum class State {
            Released,
            Pressed
        };
        QHash<quint32, quint32> buttonSerials;
        QHash<quint32, State> buttonStates;
        QPointF pos;
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QMatrix4x4 transformation;
            quint32 serial = 0;
        };
        Focus focus;
    };
    Pointer globalPointer;
    void updatePointerButtonSerial(quint32 button, quint32 serial);
    void updatePointerButtonState(quint32 button, Pointer::State state);

    // Keyboard related members
    struct Keyboard {
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
            QVector<DataDeviceInterface *> selections;
            QVector<PrimarySelectionDeviceV1Interface *> primarySelections;
        };
        Focus focus;
    };
    Keyboard globalKeyboard;

    // Touch related members
    struct Touch {
        struct Focus {
            SurfaceInterface *surface = nullptr;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QPointF firstTouchPos;
        };
        Focus focus;
        QMap<qint32, quint32> ids;
    };
    Touch globalTouch;

    struct Drag {
        enum class Mode {
            None,
            Pointer,
            Touch
        };
        Mode mode = Mode::None;
        DataDeviceInterface *source = nullptr;
        QPointer<DataDeviceInterface> target;
        SurfaceInterface *surface = nullptr;
        PointerInterface *sourcePointer = nullptr;
        TouchInterface *sourceTouch = nullptr;
        QMatrix4x4 transformation;
        QMetaObject::Connection destroyConnection;
        QMetaObject::Connection dragSourceDestroyConnection;
    };
    Drag drag;

protected:
    void seat_bind_resource(Resource *resource) override;
    void seat_get_pointer(Resource *resource, uint32_t id) override;
    void seat_get_keyboard(Resource *resource, uint32_t id) override;
    void seat_get_touch(Resource *resource, uint32_t id) override;
    void seat_release(Resource *resource) override;

private:
    void updateSelection(DataDeviceInterface *dataDevice);
    void updatePrimarySelection(PrimarySelectionDeviceV1Interface *primarySelectionDevice);

};

} // namespace KWaylandServer

#endif
