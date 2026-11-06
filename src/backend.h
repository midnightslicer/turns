#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "filepicker.h"
#include "shuffler.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(QUrl photo READ photo NOTIFY photoChanged)
    Q_PROPERTY(QString person READ person NOTIFY photoChanged)
    Q_PROPERTY(bool hasPhotos READ hasPhotos NOTIFY hasPhotosChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int seconds READ seconds NOTIFY secondsChanged)
    Q_PROPERTY(bool nameVisible READ nameVisible NOTIFY nameVisibleChanged)
    Q_PROPERTY(bool fade READ fade NOTIFY fadeChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)

public:
    explicit Backend(FilePicker *picker, const QString &settingsFile = {}, QObject *parent = nullptr);

    QString folder() const { return m_folder; }
    QUrl photo() const { return m_current.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_current); }
    QString currentPath() const { return m_current; }
    QString person() const { return m_shuffler.personOf(m_current); }
    bool hasPhotos() const { return !m_shuffler.isEmpty(); }
    bool playing() const { return m_playing; }
    int seconds() const { return m_seconds; }
    bool nameVisible() const { return m_nameVisible; }
    bool fade() const { return m_fade; }
    QString notice() const;

    // Accepts a folder, or a photo inside the folder to show.
    void openPath(const QString &path);
    // Reopens the last folder, if there was one.
    void restore();

    Q_INVOKABLE void chooseFolder();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void togglePlaying();
    Q_INVOKABLE void changeSeconds(int delta);
    Q_INVOKABLE void toggleName();
    Q_INVOKABLE void toggleFade();
    Q_INVOKABLE void photoFailed();

signals:
    void folderChanged();
    void photoChanged();
    void hasPhotosChanged();
    void playingChanged();
    void secondsChanged();
    void nameVisibleChanged();
    void fadeChanged();
    void noticeChanged();

public slots:
    // Re-announces the notice, which is translated as it's read.
    void retranslate() { emit noticeChanged(); }

private:
    void scan();
    void folderChangedOnDisk();
    void setCurrent(const QString &path);
    void restartTimer();

    FilePicker *m_picker;
    QSettings m_settings;
    Shuffler m_shuffler;
    QFileSystemWatcher m_watcher;
    QTimer m_rescan, m_timer;

    QString m_folder;
    QString m_current;
    // Everything shown so far, so Left can walk back through it.
    QStringList m_history;
    int m_position = -1;
    bool m_playing = true;
    int m_seconds = 5;
    bool m_nameVisible = true;
    bool m_fade = true;

    bool m_folderMissing = false;
    int m_withoutName = 0;
    int m_unsupported = 0;
    // Photos that wouldn't decode, in the order they failed. They stay out of
    // the slideshow until another folder is opened.
    QStringList m_broken;
    QString m_pickerError;
};
