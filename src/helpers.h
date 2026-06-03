#ifndef HELPERS_H
#define HELPERS_H

#include <QString>
#include <QFileInfo>

struct DesktopEntry {
    QString id;
    QString name;
    QString icon;
    QString exec;
    QString path;
    QString workDir;
    bool isValid = false;
};

bool isTextFile(const QString &filePath);
QString cleanFileName(const QString &fileName);
QString formatAdaptiveSize(quint64 bytes);
quint32 calculateCRC32(const QString &filePath);

DesktopEntry getDesktopEntryById(const QString &id);
DesktopEntry getDesktopEntry(const QFileInfo &fileInfo);
void openFileListWithHandler(const QString &handler, const QStringList &fileList);
void launchDesktopFile(const DesktopEntry &info, const QStringList &fileList = {});
QString getDisplayName(const QFileInfo &fileInfo, bool showFileExtensions);
QString getDisplayName(const QString &filePath, bool isDir, bool showFileExtensions);
QPixmap generateThumbnail(const QString &filePath);
bool isCurrentProcessElevated();
bool onSameStorageDevice(const QString &pathA, const QString &pathB);
void createInternetShortcut(const QString &urlStr, const QString &targetDir, const QString &webTitle);

#ifdef Q_OS_WIN
bool startProcessElevatedWin(const QString &programPath, const QString &arguments);
QString argumentsToWinString(const QStringList &args);
#endif

#endif // HELPERS_H
