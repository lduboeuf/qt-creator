// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "compilerexplorereditor.h"
#include "compilerexplorerconstants.h"
#include "compilerexploreroptions.h"
#include "compilerexplorersettings.h"
#include "compilerexplorertr.h"

#include <aggregation/aggregate.h>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditoractionhandler.h>
#include <texteditor/textmark.h>

#include <projectexplorer/projectexplorerconstants.h>

#include <utils/algorithm.h>
#include <utils/hostosinfo.h>
#include <utils/layoutbuilder.h>
#include <utils/mimetypes2/mimetype.h>
#include <utils/mimeutils.h>
#include <utils/store.h>
#include <utils/utilsicons.h>

#include <QCompleter>
#include <QDockWidget>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QSplitter>
#include <QStackedLayout>
#include <QStandardItemModel>
#include <QTemporaryFile>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;
using namespace Aggregation;
using namespace TextEditor;
using namespace Utils;

namespace CompilerExplorer {

CodeEditorWidget::CodeEditorWidget(const std::shared_ptr<SourceSettings> &settings,
                                   QUndoStack *undoStack)
    : m_settings(settings)
    , m_undoStack(undoStack){};

void CodeEditorWidget::updateHighlighter()
{
    const QString ext = m_settings->languageExtension();
    if (ext.isEmpty())
        return;

    Utils::MimeType mimeType = Utils::mimeTypeForFile("foo" + ext);
    configureGenericHighlighter(mimeType);
}

class SourceTextDocument : public TextDocument
{
public:
    class OpaqueUndoCommand : public QUndoCommand
    {
    public:
        OpaqueUndoCommand(SourceTextDocument *doc)
            : m_doc(doc)
        {}
        void undo() override { m_doc->undo(); }
        void redo() override { m_doc->redo(); }
        SourceTextDocument *m_doc;
    };

    SourceTextDocument(const std::shared_ptr<SourceSettings> &settings, QUndoStack *undoStack)
    {
        setPlainText(settings->source());

        connect(this, &TextDocument::contentsChanged, this, [settings, this] {
            settings->source.setVolatileValue(plainText());
        });

        connect(&settings->source, &Utils::StringAspect::changed, this, [settings, this] {
            if (settings->source.volatileValue() != plainText())
                setPlainText(settings->source.volatileValue());
        });

        connect(this->document(), &QTextDocument::undoCommandAdded, this, [this, undoStack] {
            undoStack->push(new OpaqueUndoCommand(this));
        });
    }

    void undo() { document()->undo(); }
    void redo() { document()->redo(); }
};

JsonSettingsDocument::JsonSettingsDocument(QUndoStack *undoStack)
    : m_undoStack(undoStack)
{
    setId(Constants::CE_EDITOR_ID);
    setMimeType("application/compiler-explorer");
    connect(&m_ceSettings, &CompilerExplorerSettings::changed, this, [this] {
        emit changed();
        emit contentsChanged();
    });
    m_ceSettings.setAutoApply(false);
    m_ceSettings.setUndoStack(undoStack);
}

Core::IDocument::OpenResult JsonSettingsDocument::open(QString *errorString,
                                                       const FilePath &filePath,
                                                       const FilePath &realFilePath)
{
    if (!filePath.isReadableFile())
        return OpenResult::ReadError;

    auto contents = realFilePath.fileContents();
    if (!contents) {
        if (errorString)
            *errorString = contents.error();
        return OpenResult::ReadError;
    }

    auto result = storeFromJson(*contents);
    if (!result) {
        if (errorString)
            *errorString = result.error();
        return OpenResult::ReadError;
    }

    setFilePath(filePath);

    m_ceSettings.fromMap(*result);
    emit settingsChanged();
    return OpenResult::Success;
}

bool JsonSettingsDocument::saveImpl(QString *errorString, const FilePath &newFilePath, bool autoSave)
{
    Store store;

    if (autoSave) {
        if (m_windowStateCallback)
            m_ceSettings.windowState.setVolatileValue(m_windowStateCallback());

        m_ceSettings.volatileToMap(store);
    } else {
        if (m_windowStateCallback)
            m_ceSettings.windowState.setValue(m_windowStateCallback());

        m_ceSettings.apply();
        m_ceSettings.toMap(store);
    }

    Utils::FilePath path = newFilePath.isEmpty() ? filePath() : newFilePath;

    if (!newFilePath.isEmpty() && !autoSave) {
        setPreferredDisplayName({});
        setFilePath(newFilePath);
    }

    auto result = path.writeFileContents(jsonFromStore(store));
    if (!result && errorString) {
        *errorString = result.error();
        return false;
    }

    emit changed();
    return true;
}

bool JsonSettingsDocument::isModified() const
{
    return m_ceSettings.isDirty();
}

bool JsonSettingsDocument::setContents(const QByteArray &contents)
{
    auto result = storeFromJson(contents);
    QTC_ASSERT_EXPECTED(result, return false);

    m_ceSettings.fromMap(*result);

    emit settingsChanged();
    emit changed();
    emit contentsChanged();
    return true;
}

SourceEditorWidget::SourceEditorWidget(const std::shared_ptr<SourceSettings> &settings,
                                       QUndoStack *undoStack)
    : m_sourceSettings(settings)
{
    m_codeEditor = new CodeEditorWidget(m_sourceSettings, undoStack);

    connect(m_codeEditor, &CodeEditorWidget::gotFocus, this, &SourceEditorWidget::gotFocus);

    TextDocumentPtr document = TextDocumentPtr(new SourceTextDocument(m_sourceSettings, undoStack));

    connect(document.get(),
            &SourceTextDocument::changed,
            this,
            &SourceEditorWidget::sourceCodeChanged);

    m_codeEditor->setTextDocument(document);
    m_codeEditor->updateHighlighter();

    auto addCompilerButton = new QPushButton;
    addCompilerButton->setText(Tr::tr("Add compiler"));
    connect(addCompilerButton, &QPushButton::clicked, this, &SourceEditorWidget::addCompiler);

    auto removeSourceButton = new QPushButton;
    removeSourceButton->setIcon(Utils::Icons::EDIT_CLEAR.icon());
    removeSourceButton->setToolTip(Tr::tr("Remove source"));
    connect(removeSourceButton, &QPushButton::clicked, this, &SourceEditorWidget::remove);

    // clang-format off
    using namespace Layouting;

    Column {
        Row {
            settings->languageId,
            addCompilerButton,
            removeSourceButton,
        },
        m_codeEditor,
    }.attachTo(this);
    // clang-format on

    setWindowTitle("Source code");
    setObjectName("source_code");

    setFocusProxy(m_codeEditor);
}

QString SourceEditorWidget::sourceCode()
{
    if (m_codeEditor && m_codeEditor->textDocument())
        return QString::fromUtf8(m_codeEditor->textDocument()->contents());
    return {};
}

CompilerWidget::CompilerWidget(const std::shared_ptr<SourceSettings> &sourceSettings,
                               const std::shared_ptr<CompilerSettings> &compilerSettings)
    : m_sourceSettings(sourceSettings)
    , m_compilerSettings(compilerSettings)
{
    using namespace Layouting;
    Store map;

    m_delayTimer = new QTimer(this);
    m_delayTimer->setSingleShot(true);
    m_delayTimer->setInterval(500ms);
    connect(m_delayTimer, &QTimer::timeout, this, &CompilerWidget::doCompile);

    connect(m_compilerSettings.get(),
            &CompilerSettings::changed,
            m_delayTimer,
            qOverload<>(&QTimer::start));

    m_asmEditor = new AsmEditorWidget;
    m_asmDocument = QSharedPointer<TextDocument>(new TextDocument);
    m_asmDocument->setFilePath("asm.asm");
    m_asmEditor->setTextDocument(m_asmDocument);
    m_asmEditor->configureGenericHighlighter(Utils::mimeTypeForName("text/x-asm"));
    m_asmEditor->setReadOnly(true);

    connect(m_asmEditor, &AsmEditorWidget::gotFocus, this, &CompilerWidget::gotFocus);

    auto advButton = new QPushButton;
    QSplitter *splitter{nullptr};

    auto advDlg = new QAction;
    advDlg->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
    advDlg->setIconText(Tr::tr("Advanced Options"));
    connect(advDlg, &QAction::triggered, this, [advButton, this] {
        CompilerExplorerOptions *dlg = new CompilerExplorerOptions(*m_compilerSettings, advButton);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowFlag(Qt::WindowType::Popup);
        dlg->show();
        QRect rect = dlg->geometry();
        rect.moveTopRight(advButton->mapToGlobal(QPoint(advButton->width(), advButton->height())));
        dlg->setGeometry(rect);
    });

    connect(advButton, &QPushButton::clicked, advDlg, &QAction::trigger);
    advButton->setIcon(advDlg->icon());

    auto removeCompilerBtn = new QPushButton;
    removeCompilerBtn->setIcon(Utils::Icons::EDIT_CLEAR.icon());
    removeCompilerBtn->setToolTip(Tr::tr("Remove compiler"));
    connect(removeCompilerBtn, &QPushButton::clicked, this, &CompilerWidget::remove);

    compile(m_sourceSettings->source());

    connect(&m_sourceSettings->source, &Utils::StringAspect::volatileValueChanged, this, [this] {
        compile(m_sourceSettings->source.volatileValue());
    });

    // clang-format off
    Column {
        Row {
            m_compilerSettings->compiler,
            advButton,
            removeCompilerBtn,
        },
        Splitter {
            bindTo(&splitter),
            m_asmEditor,
            createTerminal()
        }
    }.attachTo(this);
    // clang-format on

    m_spinner = new SpinnerSolution::Spinner(SpinnerSolution::SpinnerSize::Large, this);
}

Core::SearchableTerminal *CompilerWidget::createTerminal()
{
    m_resultTerminal = new Core::SearchableTerminal();
    m_resultTerminal->setAllowBlinkingCursor(false);
    std::array<QColor, 20> colors{Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi0),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi1),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi2),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi3),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi4),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi5),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi6),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi7),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi8),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi9),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi10),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi11),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi12),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi13),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi14),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalAnsi15),

                                  Utils::creatorTheme()->color(Utils::Theme::TerminalForeground),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalBackground),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalSelection),
                                  Utils::creatorTheme()->color(Utils::Theme::TerminalFindMatch)};

    m_resultTerminal->setColors(colors);

    return m_resultTerminal;
}

void CompilerWidget::compile(const QString &source)
{
    m_source = source;
    m_delayTimer->start();
}

void CompilerWidget::doCompile()
{
    using namespace Api;

    QString compilerId = m_compilerSettings->compiler();
    if (compilerId.isEmpty())
        compilerId = "clang_trunk";

    m_spinner->setVisible(true);
    m_asmEditor->setEnabled(false);

    CompileParameters params
        = CompileParameters(compilerId)
              .source(m_source)
              .language(m_sourceSettings->languageId.volatileValue())
              .options(CompileParameters::Options()
                           .userArguments(m_compilerSettings->compilerOptions.volatileValue())
                           .compilerOptions({false, false})
                           .filters({false,
                                     m_compilerSettings->compileToBinaryObject.volatileValue(),
                                     true,
                                     m_compilerSettings->demangleIdentifiers.volatileValue(),
                                     true,
                                     m_compilerSettings->executeCode.volatileValue(),
                                     m_compilerSettings->intelAsmSyntax.volatileValue(),
                                     true,
                                     false,
                                     false,
                                     false})
                           .libraries(m_compilerSettings->libraries.volatileValue()));

    auto f = Api::compile(m_sourceSettings->apiConfigFunction()(), params);

    m_compileWatcher.reset(new QFutureWatcher<CompileResult>);

    connect(m_compileWatcher.get(), &QFutureWatcher<CompileResult>::finished, this, [this] {
        m_spinner->setVisible(false);
        m_asmEditor->setEnabled(true);

        try {
            Api::CompileResult r = m_compileWatcher->result();

            m_resultTerminal->restart();
            m_resultTerminal->writeToTerminal("\x1b[?25l", false);

            for (const auto &err : r.stdErr)
                m_resultTerminal->writeToTerminal((err.text + "\r\n").toUtf8(), false);
            for (const auto &out : r.stdOut)
                m_resultTerminal->writeToTerminal((out.text + "\r\n").toUtf8(), false);

            m_resultTerminal->writeToTerminal(
                QString("ASM generation compiler returned: %1\r\n\r\n").arg(r.code).toUtf8(), true);

            if (r.execResult) {
                for (const auto &err : r.execResult->buildResult.stdErr)
                    m_resultTerminal->writeToTerminal((err.text + "\r\n").toUtf8(), false);
                for (const auto &out : r.execResult->buildResult.stdOut)
                    m_resultTerminal->writeToTerminal((out.text + "\r\n").toUtf8(), false);

                m_resultTerminal
                    ->writeToTerminal(QString("Execution build compiler returned: %1\r\n\r\n")
                                          .arg(r.execResult->buildResult.code)
                                          .toUtf8(),
                                      true);

                if (r.execResult->didExecute) {
                    m_resultTerminal->writeToTerminal(QString("Program returned: %1\r\n")
                                                          .arg(r.execResult->code)
                                                          .toUtf8(),
                                                      true);

                    for (const auto &err : r.execResult->stdErrLines)
                        m_resultTerminal
                            ->writeToTerminal(("  \033[0;31m" + err + "\033[0m\r\n\r\n").toUtf8(),
                                              false);
                    for (const auto &out : r.execResult->stdOutLines)
                        m_resultTerminal->writeToTerminal((out + "\r\n").toUtf8(), false);
                }
            }
            qDeleteAll(m_marks);
            m_marks.clear();

            QString asmText;
            for (auto l : r.assemblyLines)
                asmText += l.text + "\n";

            m_asmDocument->setPlainText(asmText);

            int i = 0;
            for (auto l : r.assemblyLines) {
                i++;
                if (l.opcodes.empty())
                    continue;

                auto mark = new TextMark(m_asmDocument.get(), i, TextMarkCategory{"Bytes", "Bytes"});
                mark->setLineAnnotation(l.opcodes.join(' '));
                m_marks.append(mark);
            }
        } catch (const std::exception &e) {
            qCritical() << "Exception: " << e.what();
        }
    });

    m_compileWatcher->setFuture(f);
}

EditorWidget::EditorWidget(const QSharedPointer<JsonSettingsDocument> &document,
                           QUndoStack *undoStack,
                           TextEditorActionHandler &actionHandler,
                           QWidget *parent)
    : Utils::FancyMainWindow(parent)
    , m_document(document)
    , m_undoStack(undoStack)
    , m_actionHandler(actionHandler)
{
    setContextMenuPolicy(Qt::NoContextMenu);
    setAutoHideTitleBars(false);
    setDockNestingEnabled(true);
    setDocumentMode(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::TabPosition::South);

    document->setWindowStateCallback([this] { return storeFromMap(windowStateCallback()); });

    document->settings()->m_sources.setItemAddedCallback<SourceSettings>(
        [this](const std::shared_ptr<SourceSettings> &source) { addSourceEditor(source); });

    document->settings()->m_sources.setItemRemovedCallback<SourceSettings>(
        [this](const std::shared_ptr<SourceSettings> &source) { removeSourceEditor(source); });

    connect(document.get(),
            &JsonSettingsDocument::settingsChanged,
            this,
            &EditorWidget::recreateEditors);

    connect(this, &EditorWidget::gotFocus, this, [&actionHandler] {
        actionHandler.updateCurrentEditor();
    });

    m_context = new Core::IContext(this);
    m_context->setWidget(this);
    m_context->setContext(Core::Context(Constants::CE_EDITOR_CONTEXT_ID));
    Core::ICore::addContextObject(m_context);
}

EditorWidget::~EditorWidget()
{
    m_compilerWidgets.clear();
    m_sourceWidgets.clear();
}

void EditorWidget::focusInEvent(QFocusEvent *event)
{
    emit gotFocus();
    FancyMainWindow::focusInEvent(event);
}

void EditorWidget::addCompiler(const std::shared_ptr<SourceSettings> &sourceSettings,
                               const std::shared_ptr<CompilerSettings> &compilerSettings,
                               int idx,
                               QDockWidget *parentDockWidget)
{
    auto compiler = new CompilerWidget(sourceSettings, compilerSettings);
    compiler->setWindowTitle("Compiler #" + QString::number(idx));
    compiler->setObjectName("compiler_" + QString::number(idx));
    QDockWidget *dockWidget = addDockForWidget(compiler, parentDockWidget);
    dockWidget->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, dockWidget);
    m_compilerWidgets.append(dockWidget);

    connect(compiler,
            &CompilerWidget::remove,
            this,
            [sourceSettings = sourceSettings.get(), compilerSettings = compilerSettings.get()] {
                sourceSettings->compilers.removeItem(compilerSettings->shared_from_this());
            });

    connect(compiler, &CompilerWidget::gotFocus, this, [this]() {
        m_actionHandler.updateCurrentEditor();
    });
}

QVariantMap EditorWidget::windowStateCallback()
{
    auto settings = saveSettings();
    QVariantMap result;

    for (const Key &key : settings.keys()) {
        // QTBUG-116339
        if (stringFromKey(key) != "State") {
            result.insert(stringFromKey(key), settings.value(key));
        } else {
            QVariantMap m;
            m["type"] = "Base64";
            m["value"] = settings.value(key).toByteArray().toBase64();
            result.insert(stringFromKey(key), m);
        }
    }

    return result;
}

void EditorWidget::addSourceEditor(const std::shared_ptr<SourceSettings> &sourceSettings)
{
    auto sourceEditor = new SourceEditorWidget(sourceSettings, m_undoStack);
    sourceEditor->setWindowTitle("Source Code #" + QString::number(m_sourceWidgets.size() + 1));
    sourceEditor->setObjectName("source_code_editor_" + QString::number(m_sourceWidgets.size() + 1));

    QDockWidget *dockWidget = addDockForWidget(sourceEditor);
    connect(sourceEditor, &SourceEditorWidget::remove, this, [this, sourceSettings]() {
        m_undoStack->beginMacro("Remove source");
        sourceSettings->compilers.clear();
        m_document->settings()->m_sources.removeItem(sourceSettings->shared_from_this());
        m_undoStack->endMacro();

        setupHelpWidget();
    });

    connect(sourceEditor, &SourceEditorWidget::addCompiler, this, [sourceSettings]() {
        auto newCompiler = std::make_shared<CompilerSettings>(sourceSettings->apiConfigFunction());
        newCompiler->setLanguageId(sourceSettings->languageId());
        sourceSettings->compilers.addItem(newCompiler);
    });

    connect(sourceEditor, &SourceEditorWidget::gotFocus, this, [this]() {
        m_actionHandler.updateCurrentEditor();
    });

    dockWidget->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    sourceSettings->compilers.forEachItem<CompilerSettings>(
        [this, sourceSettings, dockWidget](const std::shared_ptr<CompilerSettings> &compilerSettings,
                                           int idx) {
            addCompiler(sourceSettings, compilerSettings, idx + 1, dockWidget);
        });

    sourceSettings->compilers.setItemAddedCallback<CompilerSettings>(
        [this, sourceSettings, dockWidget](
            const std::shared_ptr<CompilerSettings> &compilerSettings) {
            addCompiler(sourceSettings->shared_from_this(),
                        compilerSettings,
                        sourceSettings->compilers.size(),
                        dockWidget);
        });

    sourceSettings->compilers.setItemRemovedCallback<CompilerSettings>(
        [this, sourceSettings](const std::shared_ptr<CompilerSettings> &compilerSettings) {
            auto it = std::find_if(m_compilerWidgets.begin(),
                                   m_compilerWidgets.end(),
                                   [compilerSettings](const QDockWidget *c) {
                                       return static_cast<CompilerWidget *>(c->widget())
                                                  ->m_compilerSettings
                                              == compilerSettings;
                                   });
            QTC_ASSERT(it != m_compilerWidgets.end(), return);
            delete *it;
            m_compilerWidgets.erase(it);
        });

    m_sourceWidgets.append(dockWidget);

    setupHelpWidget();
}

void EditorWidget::removeSourceEditor(const std::shared_ptr<SourceSettings> &sourceSettings)
{
    auto it
        = std::find_if(m_sourceWidgets.begin(),
                       m_sourceWidgets.end(),
                       [sourceSettings](const QDockWidget *c) {
                           return static_cast<SourceEditorWidget *>(c->widget())->sourceSettings()
                                  == sourceSettings.get();
                       });
    QTC_ASSERT(it != m_sourceWidgets.end(), return);
    delete *it;
    m_sourceWidgets.erase(it);
}

void EditorWidget::recreateEditors()
{
    qDeleteAll(m_sourceWidgets);
    qDeleteAll(m_compilerWidgets);

    m_sourceWidgets.clear();
    m_compilerWidgets.clear();

    m_document->settings()->m_sources.forEachItem<SourceSettings>(
        [this](const auto &sourceSettings) { addSourceEditor(sourceSettings); });

    Store windowState = m_document->settings()->windowState.value();

    if (!windowState.isEmpty()) {
        QHash<Key, QVariant> hashMap;
        for (const auto &key : windowState.keys()) {
            if (key.view() != "State")
                hashMap.insert(key, windowState.value(key));
            else {
                QVariant v = windowState.value(key);
                if (v.userType() == QMetaType::QByteArray) {
                    hashMap.insert(key, v);
                } else if (v.userType() == QMetaType::QVariantMap) {
                    QVariantMap m = v.toMap();
                    if (m.value("type") == "Base64") {
                        hashMap.insert(key, QByteArray::fromBase64(m.value("value").toByteArray()));
                    }
                }
            }
        }

        restoreSettings(hashMap);
    }
}

void EditorWidget::setupHelpWidget()
{
    if (m_document->settings()->m_sources.size() == 0) {
        setCentralWidget(createHelpWidget());
    } else {
        delete takeCentralWidget();
    }
}

QWidget *EditorWidget::createHelpWidget() const
{
    using namespace Layouting;

    auto addSourceButton = new QPushButton(Tr::tr("Add source code"));
    connect(addSourceButton, &QPushButton::clicked, this, [this] {
        auto newSource = std::make_shared<SourceSettings>(
            [settings = m_document->settings()] { return settings->apiConfig(); });
        m_document->settings()->m_sources.addItem(newSource);
    });

    // clang-format off
    return Column {
        st,
        Row {
            st,
            Column {
                Tr::tr("No source code added yet. Add one using the button below."),
                Row { st, addSourceButton, st }
            },
            st,
        },
        st,
    }.emerge();
    // clang-format on
}

TextEditor::TextEditorWidget *EditorWidget::focusedEditorWidget() const
{
    for (const QDockWidget *sourceWidget : m_sourceWidgets) {
        TextEditorWidget *textEditor
            = qobject_cast<SourceEditorWidget *>(sourceWidget->widget())->textEditor();
        if (textEditor->hasFocus())
            return textEditor;
    }

    for (const QDockWidget *compilerWidget : m_compilerWidgets) {
        TextEditorWidget *textEditor
            = qobject_cast<CompilerWidget *>(compilerWidget->widget())->textEditor();
        if (textEditor->hasFocus())
            return textEditor;
    }

    return nullptr;
}

class Editor : public Core::IEditor
{
public:
    Editor(TextEditorActionHandler &actionHandler)
        : m_document(new JsonSettingsDocument(&m_undoStack))
    {
        setWidget(new EditorWidget(m_document, &m_undoStack, actionHandler));

        connect(&m_undoStack, &QUndoStack::canUndoChanged, this, [&actionHandler] {
            actionHandler.updateActions();
        });
        connect(&m_undoStack, &QUndoStack::canRedoChanged, this, [&actionHandler] {
            actionHandler.updateActions();
        });
    }

    ~Editor()
    {
        if (m_document->isModified()) {
            auto settings = m_document->settings();
            if (settings->isDirty()) {
                settings->apply();
                Utils::Store store;
                settings->toMap(store);
                QJsonDocument doc = QJsonDocument::fromVariant(Utils::mapFromStore(store));

                CompilerExplorer::settings().defaultDocument.setValue(
                    QString::fromUtf8(doc.toJson()));
            }
        }
        delete widget();
    }

    Core::IDocument *document() const override { return m_document.data(); }
    QWidget *toolBar() override { return nullptr; }

    QSharedPointer<JsonSettingsDocument> m_document;
    QUndoStack m_undoStack;
};

EditorFactory::EditorFactory()
    : m_actionHandler(Constants::CE_EDITOR_ID,
                      Constants::CE_EDITOR_CONTEXT_ID,
                      TextEditor::TextEditorActionHandler::None,
                      [](Core::IEditor *editor) -> TextEditorWidget * {
                          return static_cast<EditorWidget *>(editor->widget())->focusedEditorWidget();
                      })
{
    setId(Constants::CE_EDITOR_ID);
    setDisplayName(Tr::tr("Compiler Explorer Editor"));
    setMimeTypes({"application/compiler-explorer"});

    auto undoStackFromEditor = [](Core::IEditor *editor) -> QUndoStack * {
        if (!editor)
            return nullptr;
        return &static_cast<Editor *>(editor)->m_undoStack;
    };

    m_actionHandler.setCanUndoCallback([undoStackFromEditor](Core::IEditor *editor) {
        if (auto undoStack = undoStackFromEditor(editor))
            return undoStack->canUndo();
        return false;
    });

    m_actionHandler.setCanRedoCallback([undoStackFromEditor](Core::IEditor *editor) {
        if (auto undoStack = undoStackFromEditor(editor))
            return undoStack->canRedo();
        return false;
    });

    setEditorCreator([this]() { return new Editor(m_actionHandler); });
}

} // namespace CompilerExplorer
