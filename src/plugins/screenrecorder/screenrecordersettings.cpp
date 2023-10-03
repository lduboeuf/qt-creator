// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "screenrecordersettings.h"

#include "screenrecorderconstants.h"
#include "screenrecordertr.h"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>

#include <help/helpconstants.h>

#include <utils/fileutils.h>
#include <utils/environment.h>
#include <utils/layoutbuilder.h>
#include <utils/utilsicons.h>

#include <QDesktopServices>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>

using namespace Utils;

namespace ScreenRecorder::Internal {

ScreenRecorderSettings &settings()
{
    static ScreenRecorderSettings theSettings;
    return theSettings;
}

static QRect stringListToRect(const QStringList &stringList)
{
    return stringList.count() == 4 ? QRect(stringList[0].toInt(), stringList[1].toInt(),
                                           stringList[2].toInt(), stringList[3].toInt())
                                   : QRect();
}

static QStringList rectToStringList(const QRect &rect)
{
    return {QString::number(rect.x()), QString::number(rect.y()),
            QString::number(rect.width()), QString::number(rect.height())};
}

ScreenRecorderSettings::ScreenRecorderSettings()
{
    setSettingsGroup("ScreenRecorder");
    setAutoApply(false);

    const QStringList versionArgs{"-version"};

    ffmpegTool.setSettingsKey("FFmpegTool");
    ffmpegTool.setExpectedKind(PathChooser::ExistingCommand);
    ffmpegTool.setCommandVersionArguments(versionArgs);
    const FilePath ffmpegDefault =
        Environment::systemEnvironment().searchInPath(Constants::FFMPEG_COMMAND);
    ffmpegTool.setDefaultValue(ffmpegDefault.toUserOutput());
    ffmpegTool.setLabelText(Tr::tr("ffmpeg tool:"));

    ffprobeTool.setSettingsKey("FFprobeTool");
    ffprobeTool.setExpectedKind(PathChooser::ExistingCommand);
    ffprobeTool.setCommandVersionArguments(versionArgs);
    const FilePath ffprobeDefault =
        Environment::systemEnvironment().searchInPath(Constants::FFPROBE_COMMAND);
    ffprobeTool.setDefaultValue(ffprobeDefault.toUserOutput());
    ffprobeTool.setLabelText(Tr::tr("ffprobe tool:"));

    captureCursor.setSettingsKey("CaptureCursor");
    captureCursor.setDefaultValue(true);
    captureCursor.setLabel(Tr::tr("Capture the mouse cursor"));
    captureCursor.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);

    captureMouseClicks.setSettingsKey("CaptureMouseClicks");
    captureMouseClicks.setDefaultValue(false);
    captureMouseClicks.setLabel(Tr::tr("Capture the screen mouse clicks"));
    captureMouseClicks.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);

    captureType.setSettingsKey("CaptureType");
    captureType.setLabelText(Tr::tr("Capture device/filter:"));
    captureType.setDisplayStyle(SelectionAspect::DisplayStyle::ComboBox);
    captureType.setVisible(false);
    switch (HostOsInfo::hostOs()) {
    case OsTypeLinux:
        captureType.addOption({"x11grab", {}, CaptureType::X11grab});
        captureType.setDefaultValue(CaptureType::X11grab);
        break;
    case OsTypeWindows:
        captureType.addOption({"ddagrab", {}, CaptureType::Ddagrab});
        captureType.setDefaultValue(CaptureType::Ddagrab);
        break;
    case OsTypeMac:
        captureType.addOption({"AVFoundation", {}, CaptureType::AVFoundation});
        captureType.setDefaultValue(CaptureType::AVFoundation);
        break;
    default:
        break;
    }
    auto setCaptureMouseClicksVisible = [this] {
        const bool visible = captureType.volatileValue() == CaptureType::AVFoundation;
        captureMouseClicks.setVisible(visible);
    };

    enableFileSizeLimit.setSettingsKey("EnableFileSizeLimit");
    enableFileSizeLimit.setDefaultValue(true);
    enableFileSizeLimit.setLabel(Tr::tr("Size limit for intermediate output file"));
    enableFileSizeLimit.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);

    fileSizeLimit.setSettingsKey("FileSizeLimit");
    fileSizeLimit.setDefaultValue(1024);
    fileSizeLimit.setRange(100, 1024 * 1024 * 2); // Up to 2GB
    fileSizeLimit.setSuffix("MB");
    fileSizeLimit.setEnabler(&enableFileSizeLimit);

    enableRtBuffer.setSettingsKey("EnableRealTimeBuffer");
    enableRtBuffer.setDefaultValue(true);
    enableRtBuffer.setLabel(Tr::tr("RAM buffer for real-time frames"));
    enableRtBuffer.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);

    rtBufferSize.setSettingsKey("RealTimeBufferSize");
    rtBufferSize.setDefaultValue(1024);
    rtBufferSize.setRange(100, 1024 * 1024 * 2); // Up to 2GB
    rtBufferSize.setSuffix("MB");
    rtBufferSize.setEnabler(&enableRtBuffer);

    logFfmpegCommandline.setSettingsKey("LogFFMpegCommandLine");
    logFfmpegCommandline.setDefaultValue(false);
    logFfmpegCommandline.setLabel(Tr::tr("Write command line of FFmpeg calls to General Messages"));
    logFfmpegCommandline.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);

    animatedImagesAsEndlessLoop.setSettingsKey("AnimatedImagesAsEndlessLoop");
    animatedImagesAsEndlessLoop.setDefaultValue(true);
    animatedImagesAsEndlessLoop.setLabel(Tr::tr("Export animated images as infinite loop"));
    animatedImagesAsEndlessLoop.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBox);

    lastOpenDirectory.setSettingsKey("LastOpenDir");
    lastOpenDirectory.setExpectedKind(PathChooser::ExistingDirectory);
    lastOpenDirectory.setDefaultValue(FileUtils::homePath().toString());

    exportLastDirectory.setSettingsKey("ExportLastDir");
    exportLastDirectory.setExpectedKind(PathChooser::ExistingDirectory);
    exportLastDirectory.setDefaultValue(FileUtils::homePath().toString());

    exportLastFormat.setSettingsKey("ExportLastFormat");
    exportLastFormat.setDefaultValue("WebP");

    lastSaveImageDirectory.setSettingsKey("LastSaveImageDir");
    lastSaveImageDirectory.setExpectedKind(PathChooser::ExistingDirectory);
    lastSaveImageDirectory.setDefaultValue(FileUtils::homePath().toString());

    recordFrameRate.setSettingsKey("RecordFrameRate");
    recordFrameRate.setDefaultValue(24);
    recordFrameRate.setLabelText(Tr::tr("Recording frame rate:"));
    recordFrameRate.setRange(1, 60);
    recordFrameRate.setSuffix(" fps");

    recordScreenId.setSettingsKey("RecordScreenID");
    recordScreenId.setDefaultValue(0);
    recordScreenId.setLabelText(Tr::tr("Screen ID:"));

    recordScreenCropRect.setSettingsKey("RecordScreenCropRect");
    recordScreenCropRect.setDefaultValue(rectToStringList({}));

    setLayouter([this] {
        using namespace Layouting;
        auto websiteLabel = new QLabel;
        websiteLabel->setText(QString("<a href=\"%1\">%1</a>").arg(Constants::FFMPEG_DOWNLOAD_URL));
        websiteLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        websiteLabel->setOpenExternalLinks(true);

        // clang-format off
        using namespace Layouting;
        return Column {
            Group {
                title(Tr::tr("FFmpeg installation")),
                Form {
                    ffmpegTool, br,
                    ffprobeTool, br,
                    websiteLabel, br,
                },
            },
            Group {
                title(Tr::tr("Record settings")),
                Column {
                    captureCursor,
                    captureMouseClicks,
                    Row { captureType, st },
                    Row { enableFileSizeLimit, fileSizeLimit, st },
                    Row { enableRtBuffer, rtBufferSize, st },
                },
            },
            Group {
                title(Tr::tr("Export settings")),
                Column {
                    animatedImagesAsEndlessLoop,
                },
            },
            logFfmpegCommandline,
            st,
        };
        // clang-format on
    });

    readSettings();

    setCaptureMouseClicksVisible();
    connect(&captureType, &SelectionAspect::volatileValueChanged, this,
            setCaptureMouseClicksVisible);
}

bool ScreenRecorderSettings::toolsRegistered() const
{
    return ffmpegTool().isExecutableFile() && ffprobeTool().isExecutableFile();
}

ScreenRecorderSettings::RecordSettings ScreenRecorderSettings::sanitizedRecordSettings(const RecordSettings &settings)
{
    const int screenIdFromSettings = settings.screenId;
    const QList<QScreen*> screens = QGuiApplication::screens();
    const int effectiveScreenId = qMin(screenIdFromSettings, screens.size() - 1);
    const QScreen *screen = screens.at(effectiveScreenId);
    const QSize screenSize = screen->size() * screen->devicePixelRatio();
    const QRect screenRect(QPoint(), screenSize);
    const QRect cropRectFromSettings = settings.cropRect;
    const QRect effectiveCropRect = screenIdFromSettings == effectiveScreenId
                                        ? screenRect.intersected(cropRectFromSettings) : QRect();
    return {effectiveScreenId, effectiveCropRect, settings.frameRate};
}

ScreenRecorderSettings::RecordSettings ScreenRecorderSettings::recordSettings() const
{
    return sanitizedRecordSettings({int(recordScreenId()), stringListToRect(recordScreenCropRect()),
                                    int(recordFrameRate())});
}

void ScreenRecorderSettings::applyRecordSettings(const RecordSettings &settings)
{
    recordScreenId.setValue(settings.screenId);
    recordScreenId.apply();
    recordScreenId.writeToSettingsImmediatly();
    recordScreenCropRect.setValue(rectToStringList(settings.cropRect));
    recordScreenCropRect.apply();
    recordScreenCropRect.writeToSettingsImmediatly();
    recordFrameRate.setValue(settings.frameRate);
    recordFrameRate.apply();
    recordFrameRate.writeToSettingsImmediatly();
}

class ScreenRecorderSettingsPage : public Core::IOptionsPage
{
public:
    ScreenRecorderSettingsPage()
    {
        setId(Constants::TOOLSSETTINGSPAGE_ID);
        setDisplayName(Tr::tr("Screen Recording"));
        setCategory(Help::Constants::HELP_CATEGORY);
        setSettingsProvider([] { return &settings(); });
    }
};

static const ScreenRecorderSettingsPage settingsPage;

} // ImageViewer::Internal
