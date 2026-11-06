#pragma once

#include <QMap>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>
#include <random>

// Decides which photo comes next. Each person has a shuffled queue of their
// own photos, and the people themselves take turns from a shuffled round, so
// someone with few photos repeats sooner but never gets more screen time, and
// nobody appears twice in a row.
class Shuffler {
public:
    explicit Shuffler(quint32 seed = QRandomGenerator::global()->generate());

    // Replaces the set of photos, keeping each person's place in their queue
    // so that a change to the folder doesn't restart the cycle.
    void setPhotos(const QStringList &paths);

    QString next();

    // The next draw won't be this photo's person. Used when a drawn photo never
    // made it to the screen, so whoever is still showing is the one to avoid.
    void avoidPersonOf(const QString &path);

    bool isEmpty() const { return m_people.isEmpty(); }
    QStringList people() const;
    QString personOf(const QString &path) const;

    // The name is everything before the first dash: "Ada-12.jpg" is Ada and
    // "Anne-Marie-3.jpg" is Anne. Empty when there's no dash or nothing before it.
    static QString personFromFileName(const QString &fileName);

private:
    struct Person {
        QString name;
        QStringList photos;
        QStringList queue;
        QString last;
    };

    static QString keyFor(const QString &name) { return name.toCaseFolded(); }
    QString draw(QStringList &queue, const QStringList &pool, const QString &avoid);
    void insertAtRandom(QStringList &list, const QString &value);
    int randomIndex(int from, int to);

    std::mt19937 m_random;
    QMap<QString, Person> m_people;
    QStringList m_round;
    QString m_lastPerson;
};
