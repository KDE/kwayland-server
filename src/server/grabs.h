/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KWAYLAND_SERVER_GRABS_P_H
#define KWAYLAND_SERVER_GRABS_P_H

#include <QMouseEvent>
#include <QKeyEvent>
#include <QPointF>
#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

/**

How This Design Came To Be
==========================

While authoring a grab implementation for use by KWin in KWayland Server, Janet
tried a variety of designs, none of which were received well by the reviewers
(Vlad Z. & David E.).

The previous designs mostly centered around the idea of the fact that a seat should
have facilities for "sticking" focus to a surface; ignoring calls to update focus
while a surface had sticky focus. This was particularly complex to implement, and
had issues, mostly relating to the interaction of the sticky focus API with compositors.
Sticky focus would come in at the tail end of the usual event flow, which would
cause situations like this:

  +
  | Compositor receives event from hardware (most likely via libinput)
  |
  | A pointer event is found to be on the edge of a window, so compositor tries
  | to clear pointer focus by setting the focused surface to nullptr.
  |
  | The seat ignores the request to set the focused surface to nullptr because
  | a sticky focus is in effect; causing a discrepancy between the compositor
  | and KWayland Server and the state given to clients. The compositor is currently
  | eating events, so no client should be marked as receiving them; yet the library
  | ignored the request to clear this, causing a client to think it should be
  | receiving events when it shouldn't be.
  v

This is not a good design; so another design was chosen: event filters. KWayland
Server would provide event filter objects to the compositor; who would then
adapt them to their event filtering models. This avoids the bug of the aforementioned
model like so:

  +
  | Compositor receives event from hardware (most likely via libinput)
  |
  | A grab is currently installed as an event filter; and the compositor has
  | decided to filter events using grabs before any other things.
  |
  | Since a grab is currently active, it eats the event filtered through it by
  | the compositor and the event stops going through the event pipeline,
  | causing the event to never hit "pointer at edge of window handling"
  | which avoids the state discrepancy described above.
  v

Besides being less buggy, the event filter model also avoids assuming all grabs
are associated with a surface; which is an assumption that does not hold for some
Wayland protocols, e.g. the input_method protocol (https://pontaoski.github.io/readway/input_method_unstable_v1.html#zwp_input_method_context_v1_grab_keyboard)
whose grabs are associated not with a surface, but with an input method context,
which requires forwarding events to the client who said context belongs to.

This brings us to our first class of the Grab family, the PointerGrab.

*/

class KWAYLANDSERVER_EXPORT PointerGrab : public QObject
{
    Q_OBJECT

/*

The PointerGrab is a class that as expected, is responsible for filtering pointer
events in a manner needed to implement grabs of various sorts.

We subclass from QObject here mainly for two reasons:

- memory management: QObjects are managed in a quasi-arena style, simplifiying
  memory management for compositors. By parenting a grab to the seat it belongs
  to, we can always guarantee that the grab will destruct when the seat destructs
  as well.

- signals/slots: Signals and slots are the primary way of notifiying objects of
  state changes in Qt programs; and grabs more often than not will need to be
  notified of changes. For example, the xdg_popup grab will want to listen to
  the xdg_popup it came from to see if it destructs, and on xdg_popups destructing,
  it will adjust its internal state as necessary.

*/

public:
    PointerGrab(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PointerGrab() {}

/*

QMouseEvents are the standard way of representing pointer events in Qt, and offer
a shared abstraction layer for compositors to communicate pointer events to grabs.
We also provide the native event code, which simplifies grab objects that want
to post the event through the wire without needing to convert the QMouseEvent's
information.

*/
    virtual bool pointerEvent(QMouseEvent *event, quint32 nativeButton);

/*

Likewise with the pointer events, the QWheelEvent* offers a shared abstraction layer
between compositor and grabs, meaning that a grab does not need to know how the
compositor receives wheel input or what data structures it uses to represent them.

*/
    virtual bool wheelEvent(QWheelEvent *event);
};

/*

Pointer devices are typically paired with keyboards, which is what the KeyboardGrab
class handles. These are typically used for active popups or input methods, which need
events before any other clients.

*/

class KWAYLANDSERVER_EXPORT KeyboardGrab : public QObject
{
    Q_OBJECT

public:
    KeyboardGrab(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~KeyboardGrab() {}

    virtual bool keyEvent(QKeyEvent *event);
};

/*

Finally, we have the TouchGrab class, which is responsible for handling
touch events.

*/

class KWAYLANDSERVER_EXPORT TouchGrab : public QObject
{
    Q_OBJECT

public:
    TouchGrab(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~TouchGrab() {}

    virtual bool touchDown(qint32 id, const QPointF &pos, quint32 time);
    virtual bool touchMotion(qint32 id, const QPointF &pos, quint32 time);
    virtual bool touchUp(qint32 id, quint32 time);
};

} // namespace KWaylandServer

#endif
