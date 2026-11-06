#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "shuffler.h"
#include "theme.h"
#include "translator.h"

class FakePicker : public FilePicker {
    Q_OBJECT
public:
    using FilePicker::FilePicker;
    void openFolder(const QString &) override { ++opened; }
    int opened = 0;
};

class TurnsTests : public QObject {
    Q_OBJECT

private slots:
    void namesComeFromTheStartOfTheFileName_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("person");
        QTest::newRow("simple") << "Mara-12.jpg" << "Mara";
        QTest::newRow("first dash wins") << "Anne-Marie-3.jpg" << "Anne";
        QTest::newRow("several numbers") << "Jack-2024-12-01.jpg" << "Jack";
        QTest::newRow("two words") << "Mary Jane-3.jpg" << "Mary Jane";
        QTest::newRow("digit in name") << "R2D2-5.jpg" << "R2D2";
        QTest::newRow("underscore kept") << "Mary_Jane-3.jpg" << "Mary_Jane";
        QTest::newRow("space around dash") << "Mara - 12.jpg" << "Mara";
        QTest::newRow("dash in folder") << "/photos/2024-trip/Mara-1.jpg" << "Mara";
        QTest::newRow("dash only in folder") << "/photos/2024-trip/Mara1.jpg" << "";
        QTest::newRow("no dash") << "Mara 12.jpg" << "";
        QTest::newRow("only a number") << "1234.jpg" << "";
        QTest::newRow("nothing before dash") << "-12.jpg" << "";
    }

    void namesComeFromTheStartOfTheFileName()
    {
        QFETCH(QString, file);
        QFETCH(QString, person);
        QCOMPARE(Shuffler::personFromFileName(file), person);
    }

    void spellingsOfANameAreOnePerson()
    {
        Shuffler shuffler(1);
        shuffler.setPhotos({"Mara-1.jpg", "mara-2.jpg", "MARA-3.jpg"});
        QCOMPARE(shuffler.people(), QStringList{"Mara"});
    }

    // The heart of the app, across many seeds and uneven folders.
    void neverTheSamePersonTwiceInARow()
    {
        for (quint32 seed = 0; seed < 200; ++seed) {
            Shuffler shuffler(seed);
            shuffler.setPhotos(uneven());
            QString previous;
            for (int i = 0; i < 300; ++i) {
                const QString person = shuffler.personOf(shuffler.next());
                QVERIFY2(person != previous, qPrintable(QString("seed %1, draw %2: %3").arg(seed).arg(i).arg(person)));
                previous = person;
            }
        }
    }

    void everyPhotoShowsBeforeAnyRepeats()
    {
        for (quint32 seed = 0; seed < 200; ++seed) {
            Shuffler shuffler(seed);
            shuffler.setPhotos(uneven());

            QHash<QString, QStringList> shown;
            for (int i = 0; i < 300; ++i) {
                const QString photo = shuffler.next();
                shown[shuffler.personOf(photo)].append(photo);
            }

            for (auto it = shown.begin(); it != shown.end(); ++it) {
                const int size = QSet<QString>(it->begin(), it->end()).size();
                for (int start = 0; start + size <= it->size(); start += size) {
                    const QStringList cycle = it->mid(start, size);
                    QCOMPARE(QSet<QString>(cycle.begin(), cycle.end()).size(), size);
                }
            }
        }
    }

    void aSmallerPersonRepeatsSoonerButGetsEqualTurns()
    {
        Shuffler shuffler(7);
        shuffler.setPhotos(uneven());
        QHash<QString, int> turns;
        for (int i = 0; i < 300; ++i)
            ++turns[shuffler.personOf(shuffler.next())];
        QCOMPARE(turns.value("Mara"), 100);
        QCOMPARE(turns.value("Jack"), 100);
        QCOMPARE(turns.value("Solo"), 100);
    }

    void aPhotoDoesNotRepeatAcrossAReshuffle()
    {
        for (quint32 seed = 0; seed < 200; ++seed) {
            Shuffler shuffler(seed);
            shuffler.setPhotos({"Mara-1.jpg", "Mara-2.jpg", "Mara-3.jpg", "Jack-1.jpg"});
            QString previousMara;
            for (int i = 0; i < 100; ++i) {
                const QString photo = shuffler.next();
                if (shuffler.personOf(photo) != "Mara")
                    continue;
                QVERIFY(photo != previousMara);
                previousMara = photo;
            }
        }
    }

    void onePersonStillPlays()
    {
        Shuffler shuffler(1);
        shuffler.setPhotos({"Mara-1.jpg", "Mara-2.jpg"});
        QSet<QString> seen{shuffler.next(), shuffler.next()};
        QCOMPARE(seen.size(), 2);
    }

    void emptyGivesNothing()
    {
        Shuffler shuffler(1);
        QVERIFY(shuffler.next().isNull());
    }

    void changingPhotosKeepsTheRules()
    {
        for (quint32 seed = 0; seed < 200; ++seed) {
            Shuffler shuffler(seed);
            QStringList photos{"Ana-1.jpg", "Ana-2.jpg", "Ben-1.jpg", "Cal-1.jpg"};
            shuffler.setPhotos(photos);
            QString previous = shuffler.personOf(shuffler.next());
            auto step = [&] {
                const QString photo = shuffler.next();
                QVERIFY(photos.contains(photo));
                const QString person = shuffler.personOf(photo);
                QVERIFY(person != previous);
                previous = person;
            };
            step();
            photos = {"Ana-1.jpg", "Ben-1.jpg", "Dee-1.jpg"};
            shuffler.setPhotos(photos);
            for (int i = 0; i < 20; ++i)
                step();
            photos = {"Ana-1.jpg", "Dee-1.jpg"};
            shuffler.setPhotos(photos);
            for (int i = 0; i < 20; ++i)
                step();
        }
    }

    void opensAFolderAndStartsPlaying()
    {
        QTemporaryDir dir;
        photo(dir, "Mara-1.png");
        photo(dir, "Jack-1.png");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));

        backend.openPath(dir.path());
        QVERIFY(backend.hasPhotos());
        QVERIFY(backend.playing());
        QVERIFY(!backend.currentPath().isEmpty());
        QCOMPARE(backend.notice(), QString());
    }

    void timerAdvancesAndPauseStopsIt()
    {
        QTemporaryDir dir;
        photo(dir, "Mara-1.png");
        photo(dir, "Jack-1.png");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));
        backend.openPath(dir.path());
        backend.changeSeconds(-60);
        QCOMPARE(backend.seconds(), 1);

        QSignalSpy changes(&backend, &Backend::photoChanged);
        QVERIFY(changes.wait(1500));

        backend.togglePlaying();
        changes.clear();
        QVERIFY(!changes.wait(1300));
    }

    void leftAndRightWalkTheHistory()
    {
        QTemporaryDir dir;
        for (const char *name : {"Mara-1.png", "Mara-2.png", "Jack-1.png", "Jack-2.png"})
            photo(dir, name);
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));
        backend.openPath(dir.path());

        const QString first = backend.currentPath();
        backend.next();
        const QString second = backend.currentPath();
        backend.next();
        const QString third = backend.currentPath();

        backend.previous();
        QCOMPARE(backend.currentPath(), second);
        backend.previous();
        QCOMPARE(backend.currentPath(), first);
        backend.previous();
        QCOMPARE(backend.currentPath(), first);
        backend.next();
        backend.next();
        QCOMPARE(backend.currentPath(), third);
    }

    void reportsWhatItSkipped()
    {
        QTemporaryDir dir;
        photo(dir, "Mara-1.png");
        photo(dir, "Jack-1.png");
        photo(dir, "1234.png");
        touch(dir, "Mara-2.heic");
        touch(dir, "notes.txt");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));
        backend.openPath(dir.path());

        QCOMPARE(backend.notice(), QString("Skipped, no name: 1\nSkipped, unsupported format: 1"));
    }

    void reportsAFolderWithNothingToShow()
    {
        QTemporaryDir dir;
        touch(dir, "notes.txt");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));

        backend.openPath(dir.path());
        QVERIFY(!backend.hasPhotos());
        QVERIFY(backend.notice().startsWith("No photos in "));

        backend.openPath(dir.filePath("gone"));
        QVERIFY(backend.notice().startsWith("Folder not found: "));
    }

    void warnsWhenThereIsOnlyOneName()
    {
        QTemporaryDir dir;
        photo(dir, "Mara-1.png");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));
        backend.openPath(dir.path());
        QVERIFY(backend.notice().contains("Only one name found: Mara"));
    }

    void followsTheFolder()
    {
        QTemporaryDir dir;
        photo(dir, "Mara-1.png");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));
        backend.openPath(dir.path());
        QCOMPARE(backend.person(), QString("Mara"));

        photo(dir, "Jack-1.png");
        QTRY_VERIFY(!backend.notice().contains("Only one name"));

        QVERIFY(QFile::remove(dir.filePath("Mara-1.png")));
        QTRY_COMPARE(backend.person(), QString("Jack"));

        QVERIFY(QFile::remove(dir.filePath("Jack-1.png")));
        QTRY_VERIFY(!backend.hasPhotos());
        QVERIFY(backend.photo().isEmpty());
    }

    void neverTouchesThePhotos()
    {
        QTemporaryDir dir;
        photo(dir, "Mara-1.png");
        photo(dir, "Jack-1.png");
        const auto before = QDir(dir.path()).entryList(QDir::Files);
        {
            FakePicker picker;
            QTemporaryDir settings;
            Backend backend(&picker, settings.filePath("settings.conf"));
            backend.openPath(dir.path());
            backend.next();
        }
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), before);
    }

    void remembersTheFolderAndSettings()
    {
        QTemporaryDir dir, settings;
        photo(dir, "Mara-1.png");
        const QString file = settings.filePath("settings.conf");
        {
            FakePicker picker;
            Backend backend(&picker, file);
            backend.openPath(dir.path());
            backend.changeSeconds(3);
            backend.toggleName();
            QVERIFY(backend.fade());
            backend.toggleFade();
        }
        FakePicker picker;
        Backend backend(&picker, file);
        backend.restore();
        QCOMPARE(QFileInfo(backend.folder()).canonicalFilePath(), QFileInfo(dir.path()).canonicalFilePath());
        QCOMPARE(backend.seconds(), 8);
        QVERIFY(!backend.nameVisible());
        QVERIFY(!backend.fade());
        QVERIFY(backend.hasPhotos());
    }

    void aPickerFolderOpensAndAFailureIsShown()
    {
        QTemporaryDir dir, settings;
        photo(dir, "Mara-1.png");
        FakePicker picker;
        Backend backend(&picker, settings.filePath("settings.conf"));

        backend.chooseFolder();
        QCOMPARE(picker.opened, 1);
        emit picker.failed("no portal");
        QVERIFY(backend.notice().contains("no portal"));

        emit picker.folderSelected(dir.path());
        QVERIFY(backend.hasPhotos());
        QVERIFY(!backend.notice().contains("no portal"));
    }

    void aBrokenPhotoIsNamedAndNeverComesBack()
    {
        QTemporaryDir dir, settings;
        photo(dir, "Mara-1.png");
        photo(dir, "Jack-1.png");
        photo(dir, "Jack-2.png");
        FakePicker picker;
        Backend backend(&picker, settings.filePath("settings.conf"));
        backend.openPath(dir.path());
        while (!backend.currentPath().endsWith("Jack-2.png"))
            backend.next();

        const QString broken = backend.currentPath();
        backend.photoFailed();
        QVERIFY(backend.currentPath() != broken);
        QVERIFY(backend.notice().contains("Can't open Jack-2.png"));
        for (int i = 0; i < 20; ++i) {
            backend.next();
            QVERIFY(backend.currentPath() != broken);
        }
    }

    // The failed photo was never seen, so the person still on screen is the one
    // who mustn't come straight back.
    void aBrokenPhotoDoesNotPutTheSamePersonOnScreenTwice()
    {
        for (int round = 0; round < 40; ++round) {
            QTemporaryDir dir, settings;
            photo(dir, "Ana-1.png");
            photo(dir, "Ben-1.png");
            photo(dir, "Cal-1.png");
            FakePicker picker;
            Backend backend(&picker, settings.filePath("settings.conf"));
            backend.openPath(dir.path());
            const QString onScreen = backend.person();
            backend.next();
            backend.photoFailed();
            QVERIFY(backend.person() != onScreen);
        }
    }

    void everyPhotoBrokenEndsCleanly()
    {
        QTemporaryDir dir, settings;
        photo(dir, "Ana-1.png");
        photo(dir, "Ben-1.png");
        FakePicker picker;
        Backend backend(&picker, settings.filePath("settings.conf"));
        backend.openPath(dir.path());
        backend.photoFailed();
        backend.photoFailed();
        QVERIFY(!backend.hasPhotos());
        QVERIFY(backend.photo().isEmpty());
        QVERIFY(backend.notice().contains("No photos in"));
        QVERIFY(backend.notice().contains("Can't open"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    // Every language Omarchy offers has every string the app shows.
    void catalogsAreComplete()
    {
        const QDir catalogs(QStringLiteral(TURNS_I18N));
        const QStringList files = catalogs.entryList({"*.json"});
        QCOMPARE(files.size(), 49);
        QFile source(catalogs.filePath("da.json"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QStringList keys = QJsonDocument::fromJson(source.readAll()).object().keys();
        QVERIFY(keys.contains("Open Folder"));
        for (const auto &name : files) {
            QFile file(catalogs.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
            QVERIFY2(object.keys() == keys, qPrintable(name));
            for (const auto &key : keys) {
                const QString value = object.value(key).toString();
                QVERIFY2(!value.isEmpty(), qPrintable(name + ": " + key));
                QVERIFY2(!key.contains("%1") || value.contains("%1"), qPrintable(name + ": " + key));
            }
        }
    }

private:
    static QStringList uneven()
    {
        QStringList photos;
        for (int i = 1; i <= 7; ++i)
            photos << QString("Mara-%1.jpg").arg(i);
        for (int i = 1; i <= 2; ++i)
            photos << QString("Jack-%1.jpg").arg(i);
        photos << "Solo-1.jpg";
        return photos;
    }

    static void photo(const QTemporaryDir &dir, const QString &name)
    {
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(dir.filePath(name)));
    }

    static void touch(const QTemporaryDir &dir, const QString &name)
    {
        write(dir.filePath(name), "");
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(TurnsTests)
#include "turns_tests.moc"
