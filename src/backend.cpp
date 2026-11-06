#include "backend.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>

namespace {
constexpr int kMinSeconds = 1;
constexpr int kMaxSeconds = 60;
constexpr int kHistoryLimit = 1000;

// The extensions that count as photos, whether or not Qt can decode them here,
// so an iPhone's HEIC files are reported instead of quietly left out.
const QStringList kPhotoExtensions{"jpg", "jpeg", "png", "webp", "gif", "bmp", "tif", "tiff",
                                   "heic", "heif", "avif", "jxl"};

QString defaultSettingsFile() {
    return QDir::homePath() + "/.config/turns/turns.conf";
}
}

Backend::Backend(FilePicker *picker, const QString &settingsFile, QObject *parent)
    : QObject(parent),
      m_picker(picker),
      m_settings(settingsFile.isEmpty() ? defaultSettingsFile() : settingsFile, QSettings::IniFormat) {
    m_seconds = qBound(kMinSeconds, m_settings.value("seconds", 5).toInt(), kMaxSeconds);
    m_nameVisible = m_settings.value("nameVisible", true).toBool();
    m_fade = m_settings.value("fade", true).toBool();

    connect(&m_timer, &QTimer::timeout, this, &Backend::next);

    m_rescan.setSingleShot(true);
    m_rescan.setInterval(300);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_rescan.start(); });
    connect(&m_rescan, &QTimer::timeout, this, &Backend::folderChangedOnDisk);

    connect(m_picker, &FilePicker::folderSelected, this, &Backend::openPath);
    connect(m_picker, &FilePicker::failed, this, [this](const QString &message) {
        m_pickerError = message;
        emit noticeChanged();
    });
}

QString Backend::notice() const {
    QStringList lines;
    if (!m_pickerError.isEmpty())
        lines << tr("The folder picker failed: %1").arg(m_pickerError);
    if (m_folderMissing)
        lines << tr("Folder not found: %1").arg(m_folder);
    else if (!m_folder.isEmpty() && !hasPhotos())
        lines << tr("No photos in %1").arg(m_folder);
    if (m_shuffler.people().size() == 1)
        lines << tr("Only one name found: %1. Photos can't alternate.").arg(m_shuffler.people().first());
    if (m_withoutName > 0)
        lines << tr("Skipped, no name: %1").arg(QLocale().toString(m_withoutName));
    if (m_unsupported > 0)
        lines << tr("Skipped, unsupported format: %1").arg(QLocale().toString(m_unsupported));
    if (!m_broken.isEmpty()) {
        QStringList names;
        for (const auto &path : m_broken)
            names << QFileInfo(path).fileName();
        lines << tr("Can't open %1").arg(names.join(", "));
    }
    return lines.join('\n');
}

void Backend::restore() {
    const QString last = m_settings.value("folder").toString();
    if (!last.isEmpty())
        openPath(last);
}

void Backend::openPath(const QString &path) {
    const QFileInfo info(path);
    m_folder = info.isFile() ? info.absolutePath() : info.absoluteFilePath();
    m_shuffler = Shuffler();
    m_history.clear();
    m_position = -1;
    m_pickerError.clear();
    m_broken.clear();
    setCurrent({});
    scan();
    if (!m_folderMissing)
        m_settings.setValue("folder", m_folder);
    emit folderChanged();
    if (hasPhotos())
        next();
    restartTimer();
    emit noticeChanged();
}

void Backend::chooseFolder() {
    m_pickerError.clear();
    emit noticeChanged();
    m_picker->openFolder(tr("Open Folder"));
}

void Backend::scan() {
    const bool hadPhotos = hasPhotos();

    // Deleting and recreating a folder drops it from the watch, so re-arm it.
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_folderMissing = !QFileInfo(m_folder).isDir();
    if (!m_folderMissing)
        m_watcher.addPath(m_folder);

    QStringList supported;
    for (const auto &format : QImageReader::supportedImageFormats())
        supported << QString::fromLatin1(format).toLower();

    QStringList photos;
    m_withoutName = 0;
    m_unsupported = 0;
    const auto entries = QDir(m_folder).entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const auto &entry : entries) {
        const QString extension = entry.suffix().toLower();
        if (!kPhotoExtensions.contains(extension))
            continue;
        if (!supported.contains(extension))
            ++m_unsupported;
        else if (Shuffler::personFromFileName(entry.fileName()).isEmpty())
            ++m_withoutName;
        else if (!m_broken.contains(entry.absoluteFilePath()))
            photos << entry.absoluteFilePath();
    }
    m_shuffler.setPhotos(photos);

    // Keep the history pointing at what's still there.
    for (int i = m_history.size() - 1; i >= 0; --i) {
        if (photos.contains(m_history[i]))
            continue;
        m_history.removeAt(i);
        if (i <= m_position)
            --m_position;
    }

    if (hadPhotos != hasPhotos())
        emit hasPhotosChanged();
    emit noticeChanged();
}

void Backend::folderChangedOnDisk() {
    scan();
    if (!hasPhotos()) {
        setCurrent({});
    } else if (m_current.isEmpty() || !QFileInfo::exists(m_current)) {
        next();
    }
    restartTimer();
}

void Backend::next() {
    if (!hasPhotos())
        return;
    if (m_position + 1 < m_history.size()) {
        ++m_position;
    } else {
        m_history.append(m_shuffler.next());
        if (m_history.size() > kHistoryLimit)
            m_history.removeFirst();
        m_position = m_history.size() - 1;
    }
    setCurrent(m_history[m_position]);
    restartTimer();
}

void Backend::previous() {
    if (m_position <= 0)
        return;
    --m_position;
    setCurrent(m_history[m_position]);
    restartTimer();
}

void Backend::togglePlaying() {
    m_playing = !m_playing;
    emit playingChanged();
    restartTimer();
}

void Backend::changeSeconds(int delta) {
    const int seconds = qBound(kMinSeconds, m_seconds + delta, kMaxSeconds);
    if (seconds == m_seconds)
        return;
    m_seconds = seconds;
    m_settings.setValue("seconds", m_seconds);
    emit secondsChanged();
    restartTimer();
}

void Backend::toggleName() {
    m_nameVisible = !m_nameVisible;
    m_settings.setValue("nameVisible", m_nameVisible);
    emit nameVisibleChanged();
}

void Backend::toggleFade() {
    m_fade = !m_fade;
    m_settings.setValue("fade", m_fade);
    emit fadeChanged();
}

// A photo that won't decode never reached the screen, so the person before it
// is still showing, and the replacement has to avoid them rather than the person
// who failed. Dropping the file also means this can't spin: each failure leaves
// one photo fewer.
void Backend::photoFailed() {
    if (m_current.isEmpty())
        return;
    m_broken << m_current;
    scan();
    if (m_position >= 0)
        m_shuffler.avoidPersonOf(m_history[m_position]);
    if (hasPhotos())
        next();
    else
        setCurrent({});
    restartTimer();
}

void Backend::setCurrent(const QString &path) {
    if (path == m_current)
        return;
    m_current = path;
    emit photoChanged();
}

void Backend::restartTimer() {
    m_timer.setInterval(m_seconds * 1000);
    if (m_playing && hasPhotos())
        m_timer.start();
    else
        m_timer.stop();
}
