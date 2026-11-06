#include "shuffler.h"

#include <QFileInfo>
#include <algorithm>

Shuffler::Shuffler(quint32 seed) : m_random(seed) {}

QString Shuffler::personFromFileName(const QString &fileName) {
    // Only the file's own name counts, since a folder's path can contain dashes.
    const QString stem = QFileInfo(fileName).completeBaseName();
    const int dash = stem.indexOf('-');
    return dash < 0 ? QString() : stem.left(dash).trimmed();
}

int Shuffler::randomIndex(int from, int to) {
    return std::uniform_int_distribution<int>(from, to)(m_random);
}

void Shuffler::insertAtRandom(QStringList &list, const QString &value) {
    list.insert(randomIndex(0, list.size()), value);
}

void Shuffler::setPhotos(const QStringList &paths) {
    QMap<QString, Person> people;
    for (const auto &path : paths) {
        const QString name = personFromFileName(path);
        if (name.isEmpty())
            continue;
        Person &person = people[keyFor(name)];
        if (person.name.isEmpty())
            person.name = name;
        person.photos.append(path);
    }

    for (auto it = people.begin(); it != people.end(); ++it) {
        const auto old = m_people.constFind(it.key());
        if (old == m_people.constEnd()) {
            insertAtRandom(m_round, it.key());
            continue;
        }
        it->last = old->last;
        for (const auto &photo : old->queue)
            if (it->photos.contains(photo))
                it->queue.append(photo);
        for (const auto &photo : it->photos)
            if (!old->photos.contains(photo))
                insertAtRandom(it->queue, photo);
    }

    m_round.removeIf([&](const QString &key) { return !people.contains(key); });
    m_people = people;
}

// Takes the front of a queue, reshuffling the pool when it runs out. The check
// is on the way out rather than at the reshuffle because it also has to hold
// after setPhotos has changed what's in the queue.
QString Shuffler::draw(QStringList &queue, const QStringList &pool, const QString &avoid) {
    if (queue.isEmpty()) {
        queue = pool;
        std::shuffle(queue.begin(), queue.end(), m_random);
    }
    if (queue.first() == avoid && queue.size() > 1)
        queue.swapItemsAt(0, randomIndex(1, queue.size() - 1));
    return queue.takeFirst();
}

QString Shuffler::next() {
    if (m_people.isEmpty())
        return {};

    // Removing photos can leave only the person just shown in the round, and
    // the only way to honor "not twice in a row" then is to start a new one.
    if (m_round.size() == 1 && m_round.first() == m_lastPerson && m_people.size() > 1)
        m_round.clear();

    m_lastPerson = draw(m_round, m_people.keys(), m_lastPerson);
    Person &person = m_people[m_lastPerson];
    person.last = draw(person.queue, person.photos, person.last);
    return person.last;
}

void Shuffler::avoidPersonOf(const QString &path) {
    m_lastPerson = keyFor(personFromFileName(path));
}

QStringList Shuffler::people() const {
    QStringList names;
    for (const auto &person : m_people)
        names.append(person.name);
    return names;
}

QString Shuffler::personOf(const QString &path) const {
    const auto found = m_people.constFind(keyFor(personFromFileName(path)));
    return found == m_people.constEnd() ? QString() : found->name;
}
