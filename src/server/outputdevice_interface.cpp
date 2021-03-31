/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputdevice_interface.h"
#include "display_p.h"
#include "display.h"
#include "logging.h"
#include "utils.h"

#include "qwayland-server-org-kde-kwin-outputdevice.h"
#include <QDebug>
#include <QString>
#include <QPointer>

namespace KWaylandServer
{

static const quint32 s_version = 3;

class OutputDeviceMode : public QtWaylandServer::org_kde_kwin_outputdevice_mode {
public:

    OutputDeviceMode(Display *display, const OutputDeviceInterface::Mode &mode) :
        QtWaylandServer::org_kde_kwin_outputdevice_mode(*display, s_version),
        m_mode(mode)
    {}

    void org_kde_kwin_outputdevice_mode_bind_resource(QtWaylandServer::org_kde_kwin_outputdevice_mode::Resource *resource) override;
    void org_kde_kwin_outputdevice_mode_destroy_resource(QtWaylandServer::org_kde_kwin_outputdevice_mode::Resource *resource) override;

    OutputDeviceInterface::Mode mode() const {
        return m_mode;
    }

    QSize size() const {
        return m_mode.size;
    }

    int refreshRate() const {
        return m_mode.refreshRate;
    }

    OutputDeviceInterface::ModeFlags flags() const {
        return m_mode.flags;
    }

private:
    OutputDeviceInterface::Mode m_mode;
};

void OutputDeviceMode::org_kde_kwin_outputdevice_mode_bind_resource(QtWaylandServer::org_kde_kwin_outputdevice_mode::Resource *resource)
{
    send_size(resource->handle, m_mode.size.width(), m_mode.size.height());
    send_refresh(resource->handle, m_mode.refreshRate);

    if (m_mode.flags.testFlag(OutputDeviceInterface::ModeFlag::Preferred)) {
        send_preferred(resource->handle);
    }
}

void OutputDeviceMode::org_kde_kwin_outputdevice_mode_destroy_resource(QtWaylandServer::org_kde_kwin_outputdevice_mode::Resource *resource)
{
    send_finished(resource->handle);
}


class OutputDeviceInterfacePrivate : public QtWaylandServer::org_kde_kwin_outputdevice
{
public:
    OutputDeviceInterfacePrivate(OutputDeviceInterface *q, Display *display);
    ~OutputDeviceInterfacePrivate() override;

    void updateGeometry();
    void updateUuid();
    void updateEdid();
    void updateEnabled();
    void updateScale();
    void updateColorCurves();
    void updateEisaId();
    void updateSerialNumber();
    void updateCapabilities();
    void updateOverscan();

    void sendGeometry(Resource *resource);
    void sendMode(Resource *resource, OutputDeviceMode &mode);
    void sendDone(Resource *resource);
    void sendUuid(Resource *resource);
    void sendEdid(Resource *resource);
    void sendEnabled(Resource *resource);
    void sendScale(Resource *resource);
    void sendColorCurves(Resource *resource);
    void sendEisaId(Resource *resource);
    void sendSerialNumber(Resource *resource);
    void sendCapabilities(Resource *resource);
    void sendOverscan(Resource *resource);

    void emitModeChanged(OutputDeviceMode newMode) const;

    static OutputDeviceInterface *get(wl_resource *native);

    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer = QStringLiteral("org.kde.kwin");
    QString model = QStringLiteral("none");
    qreal scale = 1.0;
    QString serialNumber;
    QString eisaId;
    OutputDeviceInterface::SubPixel subPixel = OutputDeviceInterface::SubPixel::Unknown;
    OutputDeviceInterface::Transform transform = OutputDeviceInterface::Transform::Normal;
    OutputDeviceInterface::ColorCurves colorCurves;

    OutputDeviceMode *currentMode;
    QList<OutputDeviceMode> modes;

    QByteArray edid;
    OutputDeviceInterface::Enablement enabled = OutputDeviceInterface::Enablement::Enabled;
    QUuid uuid;
    OutputDeviceInterface::Capabilities capabilities;
    uint32_t overscan = 0;
    QPointer<Display> display;
    OutputDeviceInterface *q;

private:
    int32_t toTransform() const;
    int32_t toSubPixel() const;

protected:
    void org_kde_kwin_outputdevice_bind_resource(Resource *resource) override;
};

OutputDeviceInterfacePrivate::OutputDeviceInterfacePrivate(OutputDeviceInterface *q, Display *display)
    : QtWaylandServer::org_kde_kwin_outputdevice(*display, s_version)
    , display(display)
    , q(q)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputdevices.append(q);
}

OutputDeviceInterfacePrivate::~OutputDeviceInterfacePrivate()
{
    if (display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
        displayPrivate->outputdevices.removeOne(q);
    }
}

OutputDeviceInterface::OutputDeviceInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputDeviceInterfacePrivate(this, display))
{
    connect(this, &OutputDeviceInterface::currentModeChanged, this,
        [this] {
            const auto clientResources = d->resourceMap();
            for (auto resource : clientResources) {
                d->sendMode(resource, *d->currentMode);
                d->sendDone(resource);
            }
        }
    );
    connect(this, &OutputDeviceInterface::subPixelChanged,       this, [this] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::transformChanged,      this, [this] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::globalPositionChanged, this, [this] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::modelChanged,          this, [this] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::manufacturerChanged,   this, [this] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::scaleFChanged,         this, [this] { d->updateScale(); });
    connect(this, &OutputDeviceInterface::colorCurvesChanged,    this, [this] { d->updateColorCurves(); });
}

OutputDeviceInterface::~OutputDeviceInterface()
{
    d->globalRemove();
}

QSize OutputDeviceInterface::pixelSize() const
{
    if (d->currentMode == nullptr) {
        return QSize();
    }
    return d->currentMode->size();
}

int OutputDeviceInterface::refreshRate() const
{
    if (d->currentMode == nullptr) {
        return 60000;
    }
    return d->currentMode->refreshRate();
}

void OutputDeviceInterface::addMode(Mode &mode)
{
    Q_ASSERT(mode.size.isValid());

    OutputDeviceMode *outputDeviceMode = nullptr;

    auto currentModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [](const OutputDeviceMode &mode) {
            return mode.mode().flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt == d->modes.end() && !mode.flags.testFlag(ModeFlag::Current)) {
        // no mode with current flag - enforce
        mode.flags |= ModeFlag::Current;
    }
    if (currentModeIt != d->modes.end() && mode.flags.testFlag(ModeFlag::Current)) {
        // another mode has the current flag - remove
        (*currentModeIt).mode().flags &= ~uint(ModeFlag::Current);
    }

    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        // remove from existing Preferred mode
        auto preferredIt = std::find_if(d->modes.begin(), d->modes.end(),
            [](const OutputDeviceMode &mode) {
                return mode.mode().flags.testFlag(ModeFlag::Preferred);
            }
        );
        if (preferredIt != d->modes.end()) {
            (*preferredIt).mode().flags &= ~uint(ModeFlag::Preferred);
        }
    }

    auto existingModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [mode](const OutputDeviceMode &mode_it) {
            return mode.size == mode_it.size() &&
                   mode.refreshRate == mode_it.refreshRate();
        }
    );
    if (existingModeIt != d->modes.end()) {
        if ((*existingModeIt).mode().flags == mode.flags) {
            // nothing to do
            return;
        }
        (*existingModeIt).mode().flags = mode.flags;
        outputDeviceMode = &(*existingModeIt);
    } else {
        outputDeviceMode = new OutputDeviceMode(d->display.data(), mode);
        d->modes << *outputDeviceMode;

        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendMode(resource, *outputDeviceMode);
            d->sendDone(resource);
        }
    }

    Q_EMIT modesChanged();
    if (mode.flags.testFlag(ModeFlag::Current)) {
        d->currentMode = outputDeviceMode;
        Q_EMIT currentModeChanged();
    }
}

void OutputDeviceInterface::setCurrentMode(OutputDeviceMode &mode)
{
    auto currentModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [](const OutputDeviceMode &mode) {
            return mode.mode().flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt != d->modes.end()) {
        // another mode has the current flag - remove
        (*currentModeIt).mode().flags &= ~uint(ModeFlag::Current);
    }

    mode.mode().flags |= ModeFlag::Current;
    d->currentMode = &mode;

    emit modesChanged();
    emit currentModeChanged();
}

bool OutputDeviceInterface::setCurrentMode(const QSize &size, int refreshRate)
{
    auto mode = std::find_if(d->modes.begin(), d->modes.end(),
        [size, refreshRate](OutputDeviceMode &mode) {
            return mode.mode().size == size && mode.mode().refreshRate == refreshRate;
        }
    );
    if (mode == d->modes.end()) {
        return false;
    }
    setCurrentMode(*mode);
    return true;
}

int32_t OutputDeviceInterfacePrivate::toTransform() const
{
    switch (transform) {
    case OutputDeviceInterface::Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case OutputDeviceInterface::Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case OutputDeviceInterface::Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case OutputDeviceInterface::Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case OutputDeviceInterface::Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case OutputDeviceInterface::Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case OutputDeviceInterface::Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case OutputDeviceInterface::Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

int32_t OutputDeviceInterfacePrivate::toSubPixel() const
{
    switch (subPixel) {
    case OutputDeviceInterface::SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case OutputDeviceInterface::SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case OutputDeviceInterface::SubPixel::HorizontalRGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case OutputDeviceInterface::SubPixel::HorizontalBGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case OutputDeviceInterface::SubPixel::VerticalRGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case OutputDeviceInterface::SubPixel::VerticalBGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    }
    abort();
}

void OutputDeviceInterfacePrivate::org_kde_kwin_outputdevice_bind_resource(Resource *resource)
{
    sendGeometry(resource);
    sendScale(resource);
    sendColorCurves(resource);
    sendEisaId(resource);
    sendSerialNumber(resource);

    auto currentModeIt = modes.end();
    for (auto it = modes.begin(); it != modes.end(); ++it) {
        OutputDeviceMode &mode = *it;
        if (mode.mode().flags.testFlag(OutputDeviceInterface::ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(resource, mode);
    }

    if (currentModeIt != modes.end()) {
        sendMode(resource, *currentModeIt);
    }

    sendUuid(resource);
    sendEdid(resource);
    sendEnabled(resource);
    sendCapabilities(resource);
    sendOverscan(resource);
    sendDone(resource);
}

void OutputDeviceInterfacePrivate::sendMode(Resource *resource, OutputDeviceMode &mode)
{
    // bind to client
    auto *clientModeResource = mode.add(resource->client(), s_version);

    send_mode(resource->handle,
                clientModeResource->handle);
}

void OutputDeviceInterfacePrivate::sendGeometry(Resource *resource)
{
    send_geometry(resource->handle,
                    globalPosition.x(),
                    globalPosition.y(),
                    physicalSize.width(),
                    physicalSize.height(),
                    toSubPixel(),
                    manufacturer,
                    model,
                    toTransform());
}

void OutputDeviceInterfacePrivate::sendScale(Resource *resource)
{
    if (resource->version() < ORG_KDE_KWIN_OUTPUTDEVICE_SCALEF_SINCE_VERSION) {
        send_scale(resource->handle, qRound(scale));
    } else {
        send_scalef(resource->handle, wl_fixed_from_double(scale));
    }
}

void OutputDeviceInterfacePrivate::sendColorCurves(Resource *resource)
{
    if (resource->version() < ORG_KDE_KWIN_OUTPUTDEVICE_COLORCURVES_SINCE_VERSION) {
        return;
    }

    QByteArray red = QByteArray::fromRawData(
        reinterpret_cast<const char *>(colorCurves.red.constData()),
        sizeof(quint16) * colorCurves.red.size()
    );

    QByteArray green = QByteArray::fromRawData(
        reinterpret_cast<const char *>(colorCurves.green.constData()),
        sizeof(quint16) * colorCurves.green.size()
    );

    QByteArray blue = QByteArray::fromRawData(
        reinterpret_cast<const char *>(colorCurves.blue.constData()),
        sizeof(quint16) * colorCurves.blue.size()
    );

    send_colorcurves(resource->handle, red, green, blue);
}

void KWaylandServer::OutputDeviceInterfacePrivate::sendSerialNumber(Resource *resource)
{
    if (resource->version() >= ORG_KDE_KWIN_OUTPUTDEVICE_SERIAL_NUMBER_SINCE_VERSION) {
        send_serial_number(resource->handle, serialNumber);
    }
}

void KWaylandServer::OutputDeviceInterfacePrivate::sendEisaId(Resource *resource)
{
    if (resource->version() >= ORG_KDE_KWIN_OUTPUTDEVICE_EISA_ID_SINCE_VERSION) {
        send_eisa_id(resource->handle, eisaId);
    }
}


void OutputDeviceInterfacePrivate::sendDone(Resource *resource)
{
    send_done(resource->handle);
}

void OutputDeviceInterfacePrivate::updateGeometry()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendGeometry(resource);
        sendDone(resource);
    }
}

void OutputDeviceInterfacePrivate::updateScale()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendScale(resource);
        sendDone(resource);
    }
}

void OutputDeviceInterfacePrivate::updateColorCurves()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendColorCurves(resource);
        sendDone(resource);
    }
}

bool OutputDeviceInterface::Mode::operator==(const Mode &mode) const
{
    return size == mode.size &&
            refreshRate == mode.refreshRate &&
            flags == mode.flags;
}
bool OutputDeviceInterface::ColorCurves::operator==(const ColorCurves &cc) const
{
    return red == cc.red && green == cc.green && blue == cc.blue;
}
bool OutputDeviceInterface::ColorCurves::operator!=(const ColorCurves &cc) const {
    return !operator==(cc);
}

#define SETTER(setterName, type, argumentName) \
    void OutputDeviceInterface::setterName(type arg) \
    { \
        if (d->argumentName == arg) { \
            return; \
        } \
        d->argumentName = arg; \
        emit argumentName##Changed(d->argumentName); \
    }

SETTER(setPhysicalSize, const QSize&, physicalSize)
SETTER(setGlobalPosition, const QPoint&, globalPosition)
SETTER(setManufacturer, const QString&, manufacturer)
SETTER(setModel, const QString&, model)
SETTER(setSerialNumber, const QString&, serialNumber)
SETTER(setEisaId, const QString&, eisaId)
SETTER(setSubPixel, SubPixel, subPixel)
SETTER(setTransform, Transform, transform)

#undef SETTER

void OutputDeviceInterface::setScaleF(qreal scale)
{
    if (qFuzzyCompare(d->scale, scale)) {
        return;
    }
    d->scale = scale;
    emit scaleFChanged(d->scale);
}

QSize OutputDeviceInterface::physicalSize() const
{
    return d->physicalSize;
}

QPoint OutputDeviceInterface::globalPosition() const
{
    return d->globalPosition;
}

QString OutputDeviceInterface::manufacturer() const
{
    return d->manufacturer;
}

QString OutputDeviceInterface::model() const
{
    return d->model;
}

QString OutputDeviceInterface::serialNumber() const
{
    return d->serialNumber;
}

QString OutputDeviceInterface::eisaId() const
{
    return d->eisaId;
}

qreal OutputDeviceInterface::scaleF() const
{
    return d->scale;
}


OutputDeviceInterface::SubPixel OutputDeviceInterface::subPixel() const
{
    return d->subPixel;
}

OutputDeviceInterface::Transform OutputDeviceInterface::transform() const
{
    return d->transform;
}

OutputDeviceInterface::ColorCurves OutputDeviceInterface::colorCurves() const
{
    return d->colorCurves;
}

void OutputDeviceInterface::setColorCurves(const ColorCurves &colorCurves)
{
    if (d->colorCurves == colorCurves) {
        return;
    }
    d->colorCurves = colorCurves;
    emit colorCurvesChanged(d->colorCurves);
}

void OutputDeviceInterface::setEdid(const QByteArray &edid)
{
    d->edid = edid;
    d->updateEdid();
    emit edidChanged();
}

QByteArray OutputDeviceInterface::edid() const
{
    return d->edid;
}

void OutputDeviceInterface::setEnabled(OutputDeviceInterface::Enablement enabled)
{
    if (d->enabled != enabled) {
        d->enabled = enabled;
        d->updateEnabled();
        emit enabledChanged();
    }
}

OutputDeviceInterface::Enablement OutputDeviceInterface::enabled() const
{
    return d->enabled;
}

void OutputDeviceInterface::setUuid(const QUuid &uuid)
{
    if (d->uuid != uuid) {
        d->uuid = uuid;
        d->updateUuid();
        emit uuidChanged();
    }
}

QUuid OutputDeviceInterface::uuid() const
{
    return d->uuid;
}

void OutputDeviceInterfacePrivate::sendEdid(Resource *resource)
{
    send_edid(resource->handle, QString::fromStdString(edid.toBase64().toStdString()));
}

void OutputDeviceInterfacePrivate::sendEnabled(Resource *resource)
{
    int32_t _enabled = 0;
    if (enabled == OutputDeviceInterface::Enablement::Enabled) {
        _enabled = 1;
    }
    send_enabled(resource->handle, _enabled);
}

void OutputDeviceInterfacePrivate::sendUuid(Resource *resource)
{
    send_uuid(resource->handle, uuid.toString(QUuid::WithoutBraces));
}

void OutputDeviceInterfacePrivate::updateEnabled()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendEnabled(resource);
    }
}

void OutputDeviceInterfacePrivate::updateEdid()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendEdid(resource);
    }
}

void OutputDeviceInterfacePrivate::updateUuid()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendUuid(resource);
    }
}

void OutputDeviceInterfacePrivate::updateEisaId()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendEisaId(resource);
    }
}

uint32_t OutputDeviceInterface::overscan() const
{
    return d->overscan;
}

OutputDeviceInterface::Capabilities OutputDeviceInterface::capabilities() const
{
    return d->capabilities;
}

void OutputDeviceInterface::setCapabilities(Capabilities cap)
{
    if (d->capabilities != cap) {
        d->capabilities = cap;
        d->updateCapabilities();
        emit capabilitiesChanged();
    }
}

void OutputDeviceInterfacePrivate::sendCapabilities(Resource *resource)
{
    if (resource->version() < ORG_KDE_KWIN_OUTPUTDEVICE_CAPABILITIES_SINCE_VERSION) {
        return;
    }
    send_capabilities(resource->handle, static_cast<uint32_t>(capabilities));
}

void OutputDeviceInterfacePrivate::updateCapabilities()
{
    const auto clientResources = resourceMap();
    for (const auto &resource : clientResources) {
        sendCapabilities(resource);
    }
}

void OutputDeviceInterface::setOverscan(uint32_t overscan)
{
    if (d->overscan != overscan) {
        d->overscan = overscan;
        d->updateOverscan();
        emit overscanChanged();
    }
}

void OutputDeviceInterfacePrivate::sendOverscan(Resource *resource)
{
    if (resource->version() < ORG_KDE_KWIN_OUTPUTDEVICE_OVERSCAN_SINCE_VERSION) {
        return;
    }
    send_overscan(resource->handle, static_cast<uint32_t>(overscan));
}

void OutputDeviceInterfacePrivate::updateOverscan()
{
    const auto clientResources = resourceMap();
    for (const auto &resource : clientResources) {
        sendOverscan(resource);
    }
}

OutputDeviceInterface *OutputDeviceInterfacePrivate::get(wl_resource *native)
{
    if (auto devicePrivate = resource_cast<OutputDeviceInterfacePrivate *>(native)) {
        return devicePrivate->q;
    }
    return nullptr;
}

OutputDeviceInterface *OutputDeviceInterface::get(wl_resource *native)
{
    return OutputDeviceInterfacePrivate::get(native);
}

}
