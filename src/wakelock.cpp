#include "wakelock.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

namespace {
const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");
constexpr uint kInhibitIdle = 8;

void closeRequest(const QString &path) {
    if (path.isEmpty())
        return;
    QDBusInterface request(kService, path, kRequest, QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}
}

void WakeLock::setActive(bool active) {
    if (m_active == active)
        return;
    m_active = active;
    ++m_generation;
    emit activeChanged();

    if (!active) {
        release();
        return;
    }

    QDBusInterface portal(kService, QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.Inhibit"), QDBusConnection::sessionBus());
    if (!portal.isValid())
        return;

    const QVariantMap options{{QStringLiteral("reason"), QStringLiteral("Showing a slideshow")}};
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(QStringLiteral("Inhibit"), QString(), kInhibitIdle, options), this);
    const int generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *done) {
        done->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *done;
        if (reply.isError())
            return;
        const QString path = reply.value().path();
        if (generation == m_generation && m_active)
            m_request = path;
        else
            closeRequest(path);
    });
}

void WakeLock::release() {
    closeRequest(m_request);
    m_request.clear();
}
