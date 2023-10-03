// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"
#include "icontext.h"

#include <utils/appmainwindow.h>
#include <utils/filepath.h>
#include <utils/qtcsettings.h>

#include <QList>
#include <QObject>
#include <QRect>
#include <QSettings>

#include <functional>

QT_BEGIN_NAMESPACE
class QColor;
class QMainWindow;
class QPrinter;
class QStatusBar;
class QWidget;
QT_END_NAMESPACE

namespace Utils { class InfoBar; }

namespace Core {

class Context;
class IDocument;
class IWizardFactory;
class NewDialog;

namespace Internal {
class MainWindow;
class MainWindowPrivate;
} // Internal

class CORE_EXPORT ICore : public QObject
{
    Q_OBJECT

    friend class Internal::MainWindow;
    friend class Internal::MainWindowPrivate;

    ICore();
    ~ICore() override;

public:
    enum class ContextPriority {
        High,
        Low
    };

    static ICore *instance();

    static bool isNewItemDialogRunning();
    static QWidget *newItemDialog();
    static void showNewItemDialog(const QString &title,
                                  const QList<IWizardFactory *> &factories,
                                  const Utils::FilePath &defaultLocation = {},
                                  const QVariantMap &extraVariables = {});

    static bool showOptionsDialog(const Utils::Id page, QWidget *parent = nullptr);
    static QString msgShowOptionsDialog();
    static QString msgShowOptionsDialogToolTip();

    static bool showWarningWithOptions(const QString &title, const QString &text,
                                       const QString &details = QString(),
                                       Utils::Id settingsId = {},
                                       QWidget *parent = nullptr);

    static Utils::QtcSettings *settings(QSettings::Scope scope = QSettings::UserScope);
    static QPrinter *printer();
    static QString userInterfaceLanguage();

    static Utils::FilePath resourcePath(const QString &rel = {});
    static Utils::FilePath userResourcePath(const QString &rel = {});
    static Utils::FilePath cacheResourcePath(const QString &rel = {});
    static Utils::FilePath installerResourcePath(const QString &rel = {});
    static Utils::FilePath libexecPath(const QString &rel = {});
    static Utils::FilePath crashReportsPath();

    static QString versionString();

    static QMainWindow *mainWindow();
    static QWidget *dialogParent();
    static Utils::InfoBar *infoBar();

    static void raiseWindow(QWidget *widget);

    static IContext *currentContextObject();
    static QWidget *currentContextWidget();
    static IContext *contextObject(QWidget *widget);
    static void updateAdditionalContexts(const Context &remove, const Context &add,
                                         ContextPriority priority = ContextPriority::Low);
    static void addAdditionalContext(const Context &context,
                                     ContextPriority priority = ContextPriority::Low);
    static void removeAdditionalContext(const Context &context);
    static void addContextObject(IContext *context);
    static void removeContextObject(IContext *context);

    static void registerWindow(QWidget *window, const Context &context);
    static void restartTrimmer();

    enum OpenFilesFlags {
        None = 0,
        SwitchMode = 1,
        CanContainLineAndColumnNumbers = 2,
         /// Stop loading once the first file fails to load
        StopOnLoadFail = 4,
        SwitchSplitIfAlreadyVisible = 8
    };
    static void openFiles(const Utils::FilePaths &filePaths, OpenFilesFlags flags = None);

    static void addPreCloseListener(const std::function<bool()> &listener);

    static void restart();

    enum SaveSettingsReason {
        SettingsDialogDone,
        ModeChanged,
        MainWindowClosing,
    };

signals:
    void coreAboutToOpen();
    void coreOpened();
    void newItemDialogStateChanged();
    void saveSettingsRequested(SaveSettingsReason reason);
    void coreAboutToClose();
    void contextAboutToChange(const QList<Core::IContext *> &context);
    void contextChanged(const Core::Context &context);
    void systemEnvironmentChanged();

public:
    /* internal use */
    static QStringList additionalAboutInformation();
    static void clearAboutInformation();
    static void appendAboutInformation(const QString &line);
    static QString systemInformation();
    static void setupScreenShooter(const QString &name, QWidget *w, const QRect &rc = QRect());
    static QString pluginPath();
    static QString userPluginPath();
    static Utils::FilePath clangExecutable(const Utils::FilePath &clangBinDirectory);
    static Utils::FilePath clangdExecutable(const Utils::FilePath &clangBinDirectory);
    static Utils::FilePath clangTidyExecutable(const Utils::FilePath &clangBinDirectory);
    static Utils::FilePath clazyStandaloneExecutable(const Utils::FilePath &clangBinDirectory);
    static Utils::FilePath clangIncludeDirectory(const QString &clangVersion,
                                                 const Utils::FilePath &clangFallbackIncludeDir);
    static QString buildCompatibilityString();
    static QStatusBar *statusBar();

    static void saveSettings(SaveSettingsReason reason);
    static void setNewDialogFactory(const std::function<NewDialog *(QWidget *)> &newFactory);
    static void updateNewItemDialogState();
};

namespace Internal {

class MainWindow : public Utils::AppMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

    void init();
    void extensionsInitialized();
    void aboutToShutdown();

    IContext *contextObject(QWidget *widget) const;
    void addContextObject(IContext *context);
    void removeContextObject(IContext *context);

    static IDocument *openFiles(const Utils::FilePaths &filePaths,
                                ICore::OpenFilesFlags flags,
                                const Utils::FilePath &workingDirectory = {});

    QPrinter *printer() const;
    IContext *currentContextObject() const;
    QStatusBar *statusBar() const;
    Utils::InfoBar *infoBar() const;

    void updateAdditionalContexts(const Context &remove, const Context &add,
                                  ICore::ContextPriority priority);

    void setOverrideColor(const QColor &color);

    QStringList additionalAboutInformation() const;
    void clearAboutInformation();
    void appendAboutInformation(const QString &line);

    void addPreCloseListener(const std::function<bool()> &listener);

    void saveSettings();

    void restart();

    void restartTrimmer();

public slots:
    static void openFileWith();
    void exit();

private:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
};

} // namespace Internal

} // namespace Core
