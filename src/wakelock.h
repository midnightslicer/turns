#pragma once

#include <QObject>
#include <QString>

// Keeps the machine from idling (screen blank, lock, suspend) while active,
// through the desktop portal's Inhibit request.
class WakeLock : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    using QObject::QObject;
    ~WakeLock() override { release(); }

    bool active() const { return m_active; }
    void setActive(bool active);

signals:
    void activeChanged();

private:
    void release();

    bool m_active = false;
    // Bumped on every change, so a reply for a stale request is let go.
    int m_generation = 0;
    QString m_request;
};
