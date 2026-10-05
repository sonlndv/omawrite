#include <QtTest>
#include <QElapsedTimer>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "backend.h"
#include "markdownhighlighter.h"

class OmawriteTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
    }

    void keepsNotoFromCrowdingTheFontList() {
        const QStringList fonts = Backend::selectableFontFamilies({
            QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Liberation Serif"),
            QStringLiteral("Noto Serif"), QStringLiteral("Noto Sans Tamil UI"),
            QStringLiteral("adwaita Sans"), QStringLiteral("IBM Plex Mono"),
            QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans Mono"),
            QStringLiteral("Liberation Serif"), QStringLiteral("Monospace"),
            QStringLiteral("Standard Symbols PS"), QStringLiteral("D050000L"),
            QStringLiteral("Noto Color Emoji"), QStringLiteral("Nimbus Sans [UKWN]"),
            QStringLiteral("Nimbus Sans [URW ]")});
        QCOMPARE(fonts, QStringList({QStringLiteral("IBM Plex Mono"),
                                     QStringLiteral("adwaita Sans"),
                                     QStringLiteral("Liberation Serif"),
                                     QStringLiteral("Nimbus Sans"),
                                     QStringLiteral("Noto Sans"),
                                     QStringLiteral("Noto Sans Mono"),
                                     QStringLiteral("Noto Serif")}));
    }

    void remembersEditorFont() {
        const QString installed = Backend().availableFonts().value(1);
        QVERIFY(!installed.isEmpty());

        {
            Backend backend;
            QCOMPARE(backend.editorFont(), Backend::defaultEditorFont());
            QSignalSpy fontSpy(&backend, &Backend::editorFontChanged);
            backend.setEditorFont(installed);
            QCOMPARE(fontSpy.count(), 1);
        }

        QCOMPARE(Backend().editorFont(), installed);

        // A remembered font that has since been uninstalled falls back.
        QSettings().setValue(QStringLiteral("editor/font"), QStringLiteral("No Such Font"));
        QCOMPARE(Backend().editorFont(), Backend::defaultEditorFont());
        QSettings().remove(QStringLiteral("editor/font"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void findsWikiLinkRanges() {
        const auto links = MarkdownHighlighter::wikiLinks(
            QStringLiteral("See [[Project Plan]] and [[Other Note|the other one]] today."));
        QCOMPARE(links.size(), 2);
        QCOMPARE(links.at(0).target, QStringLiteral("Project Plan"));
        QCOMPARE(links.at(0).whole.start, 4);
        QCOMPARE(links.at(0).whole.length, 16);
        QCOMPARE(links.at(1).target, QStringLiteral("Other Note"));
        QCOMPARE(links.at(1).whole.length, 28);
    }

    void ignoresMalformedWikiLinks() {
        QVERIFY(MarkdownHighlighter::wikiLinks(QStringLiteral("[[]]")).isEmpty());
        QVERIFY(MarkdownHighlighter::wikiLinks(QStringLiteral("[[ ]]")).isEmpty());
        QVERIFY(MarkdownHighlighter::wikiLinks(QStringLiteral("[not a link]")).isEmpty());
        const auto unterminated = MarkdownHighlighter::wikiLinks(QStringLiteral("[[Dangling"));
        QVERIFY(unterminated.isEmpty());
    }

    void resolvesWikiLinksByStemCaseInsensitiveShallowestWins() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));

        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("Root Note")));
        QVERIFY(backend.createVaultFolder(QString(), QStringLiteral("Sub")));
        QVERIFY(backend.createVaultNote(QStringLiteral("Sub"), QStringLiteral("Deep Note")));

        QCOMPARE(backend.resolveWikiLinkTarget(QStringLiteral("root note")),
                 QStringLiteral("Root Note.md"));
        QCOMPARE(backend.resolveWikiLinkTarget(QStringLiteral("Deep Note")),
                 QStringLiteral("Sub/Deep Note.md"));
        QVERIFY(backend.resolveWikiLinkTarget(QStringLiteral("No Such Note")).isEmpty());

        // Ambiguous stem at two depths: the shallower path wins.
        QVERIFY(backend.createVaultFolder(QString(), QStringLiteral("Other")));
        QVERIFY(backend.createVaultNote(QStringLiteral("Other"), QStringLiteral("Ambiguous")));
        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("Ambiguous")));
        QCOMPARE(backend.resolveWikiLinkTarget(QStringLiteral("Ambiguous")),
                 QStringLiteral("Ambiguous.md"));
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void persistsVaultRootAcrossRestart() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("myvault"));

        {
            Backend backend;
            backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));
            QCOMPARE(backend.vaultRoot(), QDir(vaultPath).absolutePath());
        }

        // Fresh instance (simulated restart) must read the same vault root
        // back out of QSettings rather than falling back to ~/notes.
        Backend restarted;
        QCOMPARE(restarted.vaultRoot(), QDir(vaultPath).absolutePath());
    }

    void syncsExternalCreateAndDeleteWithoutRestart() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));

        auto pathsInEntries = [&backend]() {
            QSet<QString> paths;
            for (const QVariant &entry : backend.vaultEntries())
                paths.insert(entry.toMap().value(QStringLiteral("path")).toString());
            return paths;
        };

        QVERIFY(!pathsInEntries().contains(QStringLiteral("external.md")));

        QFile externalFile(QDir(vaultPath).filePath(QStringLiteral("external.md")));
        QVERIFY(externalFile.open(QIODevice::WriteOnly | QIODevice::Text));
        externalFile.write("created outside the app");
        externalFile.close();

        QTRY_VERIFY(pathsInEntries().contains(QStringLiteral("external.md")));

        QVERIFY(QFile::remove(externalFile.fileName()));
        QTRY_VERIFY(!pathsInEntries().contains(QStringLiteral("external.md")));
    }

    void roundTripsNoteAndFolderCrudOnDisk() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));

        QVERIFY(backend.createVaultFolder(QString(), QStringLiteral("Folder A")));
        QVERIFY(QDir(vaultPath).exists(QStringLiteral("Folder A")));

        QVERIFY(backend.createVaultNote(QStringLiteral("Folder A"), QStringLiteral("Note One")));
        const QString originalPath = QDir(vaultPath).filePath(QStringLiteral("Folder A/Note One.md"));
        QVERIFY(QFileInfo::exists(originalPath));

        QVERIFY(backend.renameVaultEntry(QStringLiteral("Folder A/Note One.md"),
                                         QStringLiteral("Note Renamed")));
        QVERIFY(!QFileInfo::exists(originalPath));
        const QString renamedPath = QDir(vaultPath).filePath(QStringLiteral("Folder A/Note Renamed.md"));
        QVERIFY(QFileInfo::exists(renamedPath));

        QVERIFY(backend.deleteVaultEntry(QStringLiteral("Folder A/Note Renamed.md")));
        QVERIFY(!QFileInfo::exists(renamedPath));

        QVERIFY(backend.deleteVaultEntry(QStringLiteral("Folder A")));
        QVERIFY(!QDir(vaultPath).exists(QStringLiteral("Folder A")));
    }

    void treatsVaultRootItselfAsOutsideForRelativePathPurposes() {
        // Pinning the bug found and fixed during Phase 1: relativeVaultPath()
        // must report "" (not ".") for the vault root itself, and must report
        // "" for any path outside the vault, so currentVaultRelativePath()
        // correctly treats both cases as "not a vault note".
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));

        QCOMPARE(backend.relativeVaultPath(QDir(vaultPath).absolutePath()), QString());

        QTemporaryDir outside;
        QVERIFY(outside.isValid());
        const QString outsideFile = outside.filePath(QStringLiteral("elsewhere.md"));
        QFile file(outsideFile);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.close();

        QCOMPARE(backend.relativeVaultPath(outsideFile), QString());

        backend.open(QUrl::fromLocalFile(outsideFile));
        QVERIFY(backend.currentVaultRelativePath().isEmpty());
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

    void buildsLinkIndexForwardAndReverseMapsFromDisk() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));

        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("Hub")));
        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("Target")));
        QVERIFY(backend.createVaultFolder(QString(), QStringLiteral("Sub")));
        QVERIFY(backend.createVaultNote(QStringLiteral("Sub"), QStringLiteral("Deep Target")));

        // Hub links to an aliased target, a nested target, and a target that
        // does not exist anywhere in the vault.
        QFile hub(QDir(vaultPath).filePath(QStringLiteral("Hub.md")));
        QVERIFY(hub.open(QIODevice::WriteOnly | QIODevice::Text));
        hub.write("See [[Target|the target]] and [[Deep Target]] and [[Nowhere]].");
        hub.close();

        backend.open(QUrl::fromLocalFile(hub.fileName()));
        backend.rescanVaultForTest();

        const QStringList forward = backend.linkIndexForwardTargets(QStringLiteral("Hub.md"));
        QCOMPARE(forward.size(), 3);
        QVERIFY(forward.contains(QStringLiteral("Target.md")));
        QVERIFY(forward.contains(QStringLiteral("Sub/Deep Target.md")));
        QVERIFY(forward.contains(QStringLiteral("Nowhere"))); // broken link reports raw target

        QCOMPARE(backend.linkIndexBacklinks(QStringLiteral("Target.md")),
                 QStringList{QStringLiteral("Hub.md")});
        QCOMPARE(backend.linkIndexBacklinks(QStringLiteral("Sub/Deep Target.md")),
                 QStringList{QStringLiteral("Hub.md")});
        QVERIFY(backend.linkIndexBacklinks(QStringLiteral("Nowhere")).isEmpty());

        const QVariantList graph = backend.linkGraph();
        QCOMPARE(graph.size(), 3);
        bool sawBroken = false;
        for (const QVariant &edge : graph) {
            const QVariantMap map = edge.toMap();
            if (map.value(QStringLiteral("rawTarget")).toString() == QStringLiteral("Nowhere"))
                sawBroken = map.value(QStringLiteral("broken")).toBool();
        }
        QVERIFY(sawBroken);
    }

    void updatesLinkIndexOnSave() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));

        const QString mainQmlPath = QFINDTESTDATA(QStringLiteral("../src/Main.qml"));
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));

        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("A")));
        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("B")));
        QVERIFY(backend.createVaultNote(QString(), QStringLiteral("C")));

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        backend.open(QUrl::fromLocalFile(QDir(vaultPath).filePath(QStringLiteral("A.md"))));
        QVERIFY(backend.linkIndexForwardTargets(QStringLiteral("A.md")).isEmpty());

        editor->setProperty("text", QStringLiteral("Links to [[B]]."));
        backend.editorTextChanged();
        backend.save();

        QCOMPARE(backend.linkIndexForwardTargets(QStringLiteral("A.md")),
                 QStringList{QStringLiteral("B.md")});
        QCOMPARE(backend.linkIndexBacklinks(QStringLiteral("B.md")),
                 QStringList{QStringLiteral("A.md")});
        QVERIFY(backend.linkIndexBacklinks(QStringLiteral("C.md")).isEmpty());

        // Editing the link again and saving must drop the old reverse entry
        // and add the new one -- not accumulate both.
        editor->setProperty("text", QStringLiteral("Links to [[C]] now."));
        backend.editorTextChanged();
        backend.save();

        QCOMPARE(backend.linkIndexForwardTargets(QStringLiteral("A.md")),
                 QStringList{QStringLiteral("C.md")});
        QVERIFY(backend.linkIndexBacklinks(QStringLiteral("B.md")).isEmpty());
        QCOMPARE(backend.linkIndexBacklinks(QStringLiteral("C.md")),
                 QStringList{QStringLiteral("A.md")});
    }

    void rendersFiveHundredNoteGraphAtInteractiveFps() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));
        QDir().mkpath(vaultPath);

        const int noteCount = 500;
        for (int i = 0; i < noteCount; ++i) {
            QFile note(QDir(vaultPath).filePath(QStringLiteral("Note %1.md").arg(i)));
            QVERIFY(note.open(QIODevice::WriteOnly | QIODevice::Text));
            note.write(QStringLiteral("Links to [[Note %1]] and [[Note %2]].")
                           .arg((i + 1) % noteCount)
                           .arg((i + 250) % noteCount)
                           .toUtf8());
            note.close();
        }

        Backend backend;
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));
        QCOMPARE(backend.linkGraph().size(), noteCount * 2);

        const QString mainQmlPath = QFINDTESTDATA(QStringLiteral("../src/Main.qml"));
        QVERIFY(!mainQmlPath.isEmpty());

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        window->setProperty("graphVisible", true);
        QObject *graphView = window->findChild<QObject *>(QStringLiteral("graphView"));
        QVERIFY(graphView);
        QCOMPARE(graphView->property("visible").toBool(), true);

        // Drain the Qt event loop for a fixed wall-clock window so the
        // GraphView's own 16ms Timer drives real simulation + Canvas
        // repaint cycles, then measure actual frames-per-second from the
        // paint counter -- not from the cheap per-step JS cost, which says
        // nothing about paint cost.
        const int measureMs = 2000;
        QElapsedTimer timer;
        timer.start();
        int paintsAtStart = graphView->property("paintCount").toInt();
        while (timer.elapsed() < measureMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        int paintsAtEnd = graphView->property("paintCount").toInt();

        const double elapsedSeconds = timer.elapsed() / 1000.0;
        const double fps = (paintsAtEnd - paintsAtStart) / elapsedSeconds;
        qInfo("Graph view 500-note/%d-edge render: %d paints in %.2fs = %.1f fps",
              backend.linkGraph().size(), paintsAtEnd - paintsAtStart, elapsedSeconds, fps);

        QVERIFY2(fps >= 30.0, qPrintable(QStringLiteral(
            "Graph view dropped below interactive fps: %1").arg(fps)));
    }

    void reportsColdScanTimeForOneThousandNotes() {
        QTemporaryDir vaultParent;
        QVERIFY(vaultParent.isValid());
        const QString vaultPath = vaultParent.filePath(QStringLiteral("vault"));
        QDir().mkpath(vaultPath);

        for (int i = 0; i < 1000; ++i) {
            QFile note(QDir(vaultPath).filePath(QStringLiteral("Note %1.md").arg(i)));
            QVERIFY(note.open(QIODevice::WriteOnly | QIODevice::Text));
            note.write(QStringLiteral("Links to [[Note %1]] and [[Note %2]].")
                           .arg((i + 1) % 1000)
                           .arg((i + 500) % 1000)
                           .toUtf8());
            note.close();
        }

        Backend backend;
        QElapsedTimer timer;
        timer.start();
        backend.setVaultRoot(QUrl::fromLocalFile(vaultPath));
        const qint64 elapsedMs = timer.elapsed();

        qInfo("Cold scan of 1000 notes (vault scan + link index build): %lld ms", elapsedMs);
        QCOMPARE(backend.linkIndexForwardTargets(QStringLiteral("Note 0.md")).size(), 2);
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmawriteTest)
#include "tst_omawrite.moc"
