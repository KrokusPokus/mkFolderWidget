#include "fileoperation.h"

#include <QDir>
#include <QFileInfo>

FileOperation::FileOperation(QList<QUrl> urls, QString targetDir,
                             Qt::DropAction action,
                             QObject *parent)
    : QObject(parent)
    , m_urls(std::move(urls))
    , m_targetDir(std::move(targetDir))
    , m_action(action)
    , m_pendingResolution(ConflictResolution::Skip)
{}

void FileOperation::run() {
    QList<QString> items;
    for (const QUrl &url : std::as_const(m_urls)) {
        QString local = url.toLocalFile();
        if (!local.isEmpty())
            items.append(local);
    }

    const bool isCrossDevice = !items.isEmpty() && !isOnSameDevice(items.first(), m_targetDir);

    int total = items.size();
    bool anyErrors = false;

    for (int i = 0; i < items.size(); ++i) {
        emit progress(i, total);

        QString src = items.at(i);
        QFileInfo srcInfo(src);
        QString dst;

        // MoveAction into same folder blocked in MainWindow::onFilesDropped(), so we don't need to do anything special here
        if (srcInfo.absolutePath() == m_targetDir) {
            dst = generateUniqueCopyName(srcInfo, m_targetDir);
        } else {
            dst = QDir(m_targetDir).filePath(srcInfo.fileName());
        }

        FileOpResult result;
        if (srcInfo.isDir())
            result = copyOrMoveDir(src, dst, isCrossDevice); // recursive!
        else
            result = copyOrMoveFile(src, dst, isCrossDevice);

        if (result == FileOpResult::Cancelled)
            break;

        if (result == FileOpResult::Error)
            anyErrors = true;
    }

    emit progress(total, total);
    emit finished(anyErrors);
}

FileOpResult FileOperation::copyOrMoveFile(const QString &src, const QString &dst, bool isCrossDevice) {
    if (QFile::exists(dst)) {
        if (m_applyToAll) {
            // Kein Dialog – gespeicherte Entscheidung direkt anwenden
            switch (m_applyToAllResolution) {
                case ConflictResolution::Skip:     return FileOpResult::Skipped;
                case ConflictResolution::Cancel:   return FileOpResult::Cancelled;
                case ConflictResolution::Overwrite: QFile::remove(dst); break;
            }
        } else {
            Conflict conflict{ src, dst };

            m_pendingResolution = ConflictResolution::Skip;
            emit conflictDetected(conflict);    // GUI-Thread verarbeitet das via QueuedConnection
            m_semaphore.acquire();              // wartet

            // GUI-Thread schreibt m_pendingResolution via resolveConflict()
            // Kein Mutex nötig: m_semaphore.acquire() fungiert als Memory-Barrier,
            // die garantiert dass resolveConflict() vollständig abgeschlossen ist
            // bevor wir m_pendingResolution lesen.

            switch (m_pendingResolution) {
                case ConflictResolution::Skip:
                    return FileOpResult::Skipped;
                case ConflictResolution::Cancel:
                    return FileOpResult::Cancelled;
                case ConflictResolution::Overwrite:
                    QFile::remove(dst);
                    break;
            }
        }
    }

    bool ok = false;

    if (m_action == Qt::MoveAction) {
        if (!isCrossDevice) {
            ok = QFile::rename(src, dst);
            if (!ok) {
                ok = QFile::copy(src, dst);
                if (ok) {
                    QFile::remove(src);
                }
            }
        } else {
            ok = QFile::copy(src, dst);
            if (ok) {
                QFile::remove(src);
            }
        }
    } else {
        ok = QFile::copy(src, dst);
    }

    return ok ? FileOpResult::Success : FileOpResult::Error;
}

FileOpResult FileOperation::copyOrMoveDir(const QString &src, const QString &dst, bool isCrossDevice) {
    // Wenn wir einen Ordner auf demselben Laufwerk verschieben,
    // können wir uns die komplette Rekursion sparen!
    if (m_action == Qt::MoveAction && !isCrossDevice) {
        if (QFile::rename(src, dst)) {
            return FileOpResult::Success;
        }
    }

    QDir dstDir(dst);
    if (dstDir.exists()) {
        // Konflikt: Zielordner existiert bereits → nachfragen
        if (m_applyToAll) {
            // Kein Dialog – gespeicherte Entscheidung direkt anwenden
            switch (m_applyToAllResolution) {
                case ConflictResolution::Skip:     return FileOpResult::Skipped;
                case ConflictResolution::Cancel:   return FileOpResult::Cancelled;
                case ConflictResolution::Overwrite: break;
            }
        } else {
            Conflict conflict{ src, dst };

            m_pendingResolution = ConflictResolution::Skip;
            emit conflictDetected(conflict);    // GUI-Thread verarbeitet das via QueuedConnection
            m_semaphore.acquire();              // wartet

            // GUI-Thread schreibt m_pendingResolution via resolveConflict()
            // Kein Mutex nötig: m_semaphore.acquire() fungiert als Memory-Barrier,
            // die garantiert dass resolveConflict() vollständig abgeschlossen ist
            // bevor wir m_pendingResolution lesen.

            switch (m_pendingResolution) {
                case ConflictResolution::Skip:
                    return FileOpResult::Skipped;
                case ConflictResolution::Cancel:
                    return FileOpResult::Cancelled;
                case ConflictResolution::Overwrite: // = "Zusammenführen"
                    break; // weiter mit Rekursion
            }
        }
    } else {
        if (!QDir().mkpath(dst))
            return FileOpResult::Error;
    }

    QDir srcDir(src);
    const QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

    bool anyErrors = false;
    bool anySkipped = false;

    for (const QFileInfo &entry : entries) {
        QString childDst = dstDir.filePath(entry.fileName());
        FileOpResult r = entry.isDir()
                             ? copyOrMoveDir(entry.filePath(), childDst, isCrossDevice)
                             : copyOrMoveFile(entry.filePath(), childDst, isCrossDevice);

        if (r == FileOpResult::Cancelled) return FileOpResult::Cancelled;
        if (r == FileOpResult::Error)     anyErrors = true;
        if (r == FileOpResult::Skipped)   anySkipped = true;
    }

    if (!anyErrors && !anySkipped && m_action == Qt::MoveAction) {
        srcDir.removeRecursively();
    }

    if (anyErrors)   return FileOpResult::Error;
    if (anySkipped)  return FileOpResult::Skipped;

    return FileOpResult::Success;
}

void FileOperation::resolveConflict(ConflictResolution resolution, bool applyToAll) {
    m_pendingResolution = resolution;
    m_applyToAll = applyToAll;
    m_semaphore.release();
}

bool FileOperation::isOnSameDevice(const QString &src, const QString &dst) const {
    QString srcDir = QFileInfo(src).absolutePath();
    QString dstDir = QFileInfo(dst).absolutePath();

    QStorageInfo storageSrc(srcDir);
    QStorageInfo storageDst(dstDir);

    return storageSrc.isValid() && storageSrc.isReady() &&
           storageDst.isValid() && storageDst.isReady() &&
           (storageSrc == storageDst);
}

QString FileOperation::generateUniqueCopyName(const QFileInfo &srcInfo, const QString &targetDir) {
    QString base = srcInfo.completeBaseName();
    QString ext = srcInfo.suffix();
    if (!ext.isEmpty()) {
        ext = "." + ext;
    }

    int counter = 1;
    QString newPath;

    do {
        QString suffix = (counter == 1) ? tr(" (Copy)") : QString(tr(" (Copy %1)")).arg(counter);
        newPath = QDir(targetDir).filePath(base + suffix + ext);
        counter++;
    } while (QFile::exists(newPath));

    return newPath;
}
