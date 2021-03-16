/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once
#include "display.h"

namespace KWaylandServer
{
class SurfaceInterface;
class OutputInterface;
class FullscreenShellModeFeedbackV1Interface;
class FullscreenShellV1InterfacePrivate;
class FullscreenShellModeFeedbackV1InterfacePrivate;

class KWAYLANDSERVER_EXPORT FullscreenShellV1Interface : public QObject
{
    Q_OBJECT
public:
    enum Capability {
        ArbitraryModes = 1, ///< compositor is capable of almost any output mode
        CursorPlane = 2, ///< compositor has a separate cursor plane
    };
    Q_ENUM(Capability);
    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum PresentMethod {
        Default = 0, ///< no preference, apply default policy
        Center = 1, ///< center the surface on the output
        Zoom = 2, ///< scale the surface, preserving aspect ratio, to the largest size that will fit on the output
        ZoomCrop = 3, ///< scale the surface, preserving aspect ratio, to fully fill the output cropping if needed
        Stretch = 4, ///< scale the surface to the size of the output ignoring aspect ratio
    };
    Q_ENUM(PresentMethod);

    enum Error {
        error_invalid_method = 0, ///< present_method is not known
    };
    Q_ENUM(Error);

    explicit FullscreenShellV1Interface(Capabilities capabilities, Display *display, QObject *parent = nullptr);
    ~FullscreenShellV1Interface() override;

Q_SIGNALS:
    void presentSurface(PresentMethod method, SurfaceInterface *surface, OutputInterface *output);
    void presentSurfaceForMode(FullscreenShellModeFeedbackV1Interface *mode);

private:
    QScopedPointer<FullscreenShellV1InterfacePrivate> d;
};

class FullscreenShellModeFeedbackV1Interface : public QObject
{
    Q_OBJECT
public:
    ~FullscreenShellModeFeedbackV1Interface() override;

    void sendModeSuccessful();
    void sendModeFailed();
    void sendPresentCancelled();

    KWaylandServer::SurfaceInterface *surface() const;
    KWaylandServer::OutputInterface *output() const;
    int32_t framerate() const;

private:
    FullscreenShellModeFeedbackV1Interface(SurfaceInterface *surface, OutputInterface *output, int32_t framerate);
    friend class FullscreenShellV1InterfacePrivate;
    QScopedPointer<FullscreenShellModeFeedbackV1InterfacePrivate> d;
};

}
