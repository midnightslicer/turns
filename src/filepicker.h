#pragma once

#include <QObject>
#include <QString>

// Abstract so tests can inject a fake instead of opening a real dialog.
class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    virtual void openFolder(const QString &title) = 0;

signals:
    void folderSelected(const QString &path);
    void failed(const QString &message);
};
