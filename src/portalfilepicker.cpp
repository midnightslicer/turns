// Follows Omacut's portal file picker (MIT, David Heinemeier Hansson), narrowed
// to choosing a folder.
#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>

namespace {
const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");

QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}

QString startFolder() {
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return QDir(pictures).exists() ? pictures : QDir::homePath();
}
}

bool PortalFilePicker::connectToRequestPath(const QString &path) {
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(kService, m_pendingPath, kRequest, QStringLiteral("Response"),
                                                 this, SLOT(handleResponse(uint,QVariantMap)));
}

void PortalFilePicker::closePendingRequest() {
    if (m_pendingPath.isEmpty())
        return;
    QDBusInterface request(kService, m_pendingPath, kRequest, QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}

void PortalFilePicker::openFolder(const QString &title) {
    // A request whose Response never arrives (the portal died with the dialog
    // up) must not wedge every later dialog behind it, so replace it.
    closePendingRequest();
    clearPending();

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(kService, QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"), bus);
    if (!portal.isValid()) {
        emit failed(portal.lastError().message());
        return;
    }

    // Subscribe at the request path the portal will derive from our token
    // before calling, so a response can't slip past the match rule.
    const QString token = QStringLiteral("turns_%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    if (!connectToRequestPath(QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token))) {
        const QString message = bus.lastError().message();
        clearPending();
        emit failed(message);
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("accept_label"), title);
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("directory"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(startFolder()));

    const int generation = ++m_pendingGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(QStringLiteral("OpenFile"), QString(), title, options), this);
    m_pendingWatcher = watcher;

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pendingWatcher == watcher)
            m_pendingWatcher = nullptr;
        if (generation != m_pendingGeneration || m_pendingPath.isEmpty())
            return;

        if (reply.isError()) {
            clearPending();
            emit failed(reply.error().message());
            return;
        }

        // Old portal versions can hand back a different request path than the
        // token predicts; move our subscription over if so.
        const QString actualPath = reply.value().path();
        if (actualPath != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(kService, m_pendingPath, kRequest, QStringLiteral("Response"),
                                                     this, SLOT(handleResponse(uint,QVariantMap)));
            if (!connectToRequestPath(actualPath)) {
                const QString message = QDBusConnection::sessionBus().lastError().message();
                clearPending();
                emit failed(message);
            }
        }
    });
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    clearPending();
    if (response != 0)
        return;
    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (!uris.isEmpty())
        emit folderSelected(QUrl(uris.first()).toLocalFile());
}

void PortalFilePicker::clearPending() {
    if (m_pendingWatcher) {
        m_pendingWatcher->deleteLater();
        m_pendingWatcher = nullptr;
    }
    if (!m_pendingPath.isEmpty())
        QDBusConnection::sessionBus().disconnect(kService, m_pendingPath, kRequest, QStringLiteral("Response"),
                                                 this, SLOT(handleResponse(uint,QVariantMap)));
    m_pendingPath.clear();
}
