#ifndef FILEOPERATION_H
#define FILEOPERATION_H

#include <QList>
#include <QObject>
#include <QSemaphore>
#include <QStorageInfo>
#include <QString>
#include <QUrl>
#include <Qt>

enum class FileOpResult { Success, Skipped, Error, Cancelled};

struct FileConflict {
    QString sourcePath;
    QString targetPath;
};

enum class ConflictResolution { Overwrite, Skip, Cancel };

class FileOperation : public QObject {
    Q_OBJECT
public:
    explicit FileOperation(QList<QUrl> urls,
                           QString targetDir,
                           Qt::DropAction action,
                           QObject *parent = nullptr);

signals:
    void progress(int current, int total);
    void conflictDetected(const FileConflict &conflict);  // blockierend via Qt::BlockingQueuedConnection
    void finished(bool anyErrors);

public slots:
    void run();
    void resolveConflict(ConflictResolution resolution);

private:
    FileOpResult copyOrMoveFile(const QString &src, const QString &dst, bool isCrossDevice);
    FileOpResult copyOrMoveDir(const QString &src, const QString &dst, bool isCrossDevice);
    bool isOnSameDevice(const QString &src, const QString &dst) const;

    QList<QUrl>     m_urls;
    QString         m_targetDir;
    Qt::DropAction  m_action;

    ConflictResolution m_pendingResolution;
    QSemaphore m_semaphore;
};

#endif // FILEOPERATION_H
