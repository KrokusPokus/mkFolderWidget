#include "helpers.h"

#include <QDir>
#include <QImageReader>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeType>
#include <QMimeDatabase>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QRegularExpression>
#include <QUrl>

#include <zlib.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>   // needed for ShellExecuteEx in startProcessElevatedWin()
#endif

bool isTextFile(const QString &filePath) {
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath);
    return mime.inherits("text/plain");
}

QString cleanFileName(const QString &fileName) {
    QString pattern;

#ifdef Q_OS_WIN
    // Windows verbotene Zeichen: < > : " / \ | ? *
    // Plus: Steuerzeichen (0-31)
    pattern = "[<>:\"/\\\\|?*\\x00-\\x1F]";
#else
    // Linux/Unix verbotene Zeichen: / und der Null-Terminator \0
    pattern = "[/\\x00]";
#endif

    static const QRegularExpression re(pattern);

    QString cleanedName = fileName; // local copy
    cleanedName.replace(re, "_");

#ifdef Q_OS_WIN
    cleanedName = cleanedName.trimmed();
    while (cleanedName.endsWith('.')) {
        cleanedName.chop(1);
    }
#endif

    return cleanedName;
}

QString formatAdaptiveSize(quint64 bytes) {
    static const QLocale locale = QLocale::system();

    if (bytes < 1024) {
        return locale.toString(bytes) + " Bytes";
    }

    double size = static_cast<double>(bytes);
    static const QStringList units = {"Bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
    int unitIndex = 0;

    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }

    int precision = 0;
    if (size < 10.0) {
        precision = 2; // z.B. 1,23 MiB
    } else if (size < 100.0) {
        precision = 1; // z.B. 12,3 MiB
    } else {
        precision = 0; // z.B. 123 MiB
    }

    return locale.toString(size, 'f', precision) + " " + units[unitIndex];
}

quint32 calculateCRC32(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0; // Or handle error accordingly
    }

    // Initialize the CRC value
    uLong crc = crc32(0L, Z_NULL, 0);

    // Read in chunks to be memory efficient
    const int bufferSize = 1024 * 1024; // 1 MB buffer
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), bufferSize);
        // Update the CRC with the current chunk
        crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()), bytesRead);
    }

    file.close();
    return static_cast<quint32>(crc);
}

DesktopEntry getDesktopEntryById(const QString &id) {
    static const QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dirPath : appDirs) {
        QFileInfo fileInfo(QDir(dirPath).filePath(id));
        DesktopEntry entry = getDesktopEntry(fileInfo);
        if (entry.isValid) {
            return entry;
        }
    }
    return DesktopEntry();
}

DesktopEntry getDesktopEntry(const QFileInfo &fileInfo) {

    static const QString localeName = QLocale::system().name().left(2); // "de", "fr", ...
    static const QString localKey = QString("Name[%1]=").arg(localeName);

    DesktopEntry currentEntry;

    QFile file(fileInfo.filePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);

        QString iniName, iniNameLocalised;
        bool bInMainSection = false;

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();

            if (line.startsWith('[') && line.endsWith(']')) {
                if (bInMainSection) break;
                bInMainSection = (line == "[Desktop Entry]");
                continue;
            }

            if (!bInMainSection || line.startsWith('#')) continue;

            if (line.startsWith("Name=")) iniName = line.mid(5);
            else if (line.startsWith(localKey)) iniNameLocalised = line.mid(localKey.size());
            else if (line.startsWith("Icon=")) currentEntry.icon = line.mid(5);
            else if (line.startsWith("Exec=")) currentEntry.exec = line.mid(5);
            else if (line.startsWith("Path=")) currentEntry.workDir = line.mid(5);
            else if (line.startsWith("Hidden=true")) {
                return DesktopEntry();
            }
        }

        currentEntry.name = iniNameLocalised.isEmpty() ? iniName : iniNameLocalised;
        currentEntry.isValid = !currentEntry.name.isEmpty();
        currentEntry.id = fileInfo.fileName();
        currentEntry.path = fileInfo.filePath();

        if (currentEntry.isValid) {
            return currentEntry;
        }
    }

    return DesktopEntry();
}

void openFileListWithHandler(const QString &handlerApp, const QStringList &fileList) {
    if (handlerApp.endsWith(".desktop", Qt::CaseInsensitive)) {
        launchDesktopFile(getDesktopEntryById(handlerApp), fileList);
        return;
    }

    if (QFile::exists(handlerApp)) {
        QProcess::startDetached(QDir::toNativeSeparators(handlerApp), fileList);
        return;
    }

    QString absolutePath = QStandardPaths::findExecutable(handlerApp);
    if (!absolutePath.isEmpty()) {
        // bool QProcess::startDetached(const QString &program, const QStringList &arguments = {}, const QString &workingDirectory = QString(), qint64 *pid = nullptr)
        QProcess::startDetached(QDir::toNativeSeparators(absolutePath), fileList);
        return;
    }

    qDebug() << "openFileListWithHandler() No absolute path found for:" << handlerApp;
}

void launchDesktopFile(const DesktopEntry &info, const QStringList &fileList) {
    if (info.exec.isEmpty()) return;

    QStringList args = QProcess::splitCommand(info.exec);
    if (args.isEmpty()) return;
    QString program = args.takeFirst();

    if (!fileList.isEmpty()) {
        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == "%f" || args[i] == "%u") {
                args[i] = fileList.first();
            } else if (args[i] == "%F" || args[i] == "%U") {
                args.removeAt(i);
                for (int j = 0; j < fileList.size(); ++j)
                    args.insert(i + j, fileList.at(j));
                break;
            }
        }
        if (!info.exec.contains('%'))
            args.append(fileList);
    } else {
        static const QRegularExpression placeholders("%[uUfFiIcK]");
        args.removeIf([&](const QString &a) { return a.contains(placeholders); });
    }

    auto expandHome = [](QString path) -> QString {
        if (path.startsWith("$HOME") || path.startsWith('~')) {
            const QString home = QDir::homePath();
            if (path.startsWith("$HOME"))
                return path.replace("$HOME", home);
            if (path.startsWith('~'))
                return path.replace(0, 1, home);
        }
        return path;
    };

    program = expandHome(program);
    if (!QFileInfo(program).isAbsolute()) {
        program = QStandardPaths::findExecutable(program);
        if (program.isEmpty()) {
            return;
        }
    }

    QString workDir = expandHome(info.workDir.trimmed());
    if (workDir.startsWith('"') && workDir.endsWith('"'))
        workDir = workDir.mid(1, workDir.length() - 2);
    if (workDir.isEmpty() || !QDir(workDir).exists())
        workDir = QFileInfo(program).absolutePath();

    QProcess::startDetached(program, args, workDir);
}

QString getDisplayName(const QFileInfo &fileInfo, bool showFileExtensions) {
    if (showFileExtensions || fileInfo.isDir()) {
        return fileInfo.fileName();
    }

    QString displayName = fileInfo.completeBaseName();
    return displayName.isEmpty() ? fileInfo.fileName() : displayName;
}

QString getDisplayName(const QString &filePath, bool isDir, bool showFileExtensions) {
    /* Alternative way
    // QFileInfo wird hier rein als Text-Parser genutzt! Kein Disk-I/O.
    QFileInfo finfo(filePath);

    if (showFileExtensions || isDir) {
        return finfo.fileName();
    }

    QString displayName = finfo.completeBaseName();
    return displayName.isEmpty() ? finfo.fileName() : displayName;
    */

    int lastSeparator = filePath.lastIndexOf('/');
    if (lastSeparator == -1) {
        lastSeparator = filePath.lastIndexOf('\\');
    }

    QString fileName = (lastSeparator == -1) ? filePath : filePath.mid(lastSeparator + 1);

    if (showFileExtensions || isDir) {
        return fileName;
    }

    int lastDot = fileName.lastIndexOf('.');
    if (lastDot <= 0) {
        return fileName;
    }
    return fileName.sliced(0, lastDot);
}

QPixmap generateThumbnail(const QString &filePath) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true); // Wichtig für EXIF-Rotationen von Smartphones

    if (reader.canRead()) {
        // 1. Die echte Dimension des Bildes auslesen (kostet kaum Performance)
        QSize originalSize = reader.size();

        // 2. Proportionale Größe berechnen, die in eine 96x96 Box passt
        // Aus z.B. 1920x1080 wird hier automatisch 96x54
        QSize scaledSize = originalSize.scaled(QSize(96, 96), Qt::KeepAspectRatio);

        // 3. Dem Reader die proportionale Größe mitteilen
        reader.setScaledSize(scaledSize);

        QImage img = reader.read();
        if (!img.isNull()) {
            return QPixmap::fromImage(img);
        }
    }

    return QPixmap(); // Fallback
}

bool onSameStorageDevice(const QString &pathA, const QString &pathB) {
    QString folderA = QFileInfo(pathA).absolutePath();
    QString folderB = QFileInfo(pathB).absolutePath();

    QStorageInfo storageA(folderA);
    QStorageInfo storageB(folderB);

    return storageA.isValid() && storageA.isReady() &&
           storageB.isValid() && storageB.isReady() &&
           (storageA == storageB);
}

void createInternetShortcut(const QString &urlStr, const QString &targetDir, const QString &webTitle) {
    // 1. Basis-Dateinamen bestimmen (Trimmen entfernt führende/nachstehende Leerzeichen)
    QString fileName = webTitle.trimmed();

    // Fallback A: Wenn data->text() leer war, nutzen wir die Domain (z.B. "www.wikipedia.org")
    if (fileName.isEmpty()) {
        QUrl url(urlStr);
        fileName = url.host();
    }

    // Fallback B: Wenn auch die Domain fehlschlägt (z.B. ungültige URL), generischer Name
    if (fileName.isEmpty()) {
        fileName = "Internet-Link";
    }

    // 2. Dateinamen bereinigen (Sanitize)
    // Ersetzt alle im Windows/Linux/macOS-Dateisystem verbotenen Zeichen durch einen Unterstrich: \ / : * ? " < > |
    fileName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    fileName = fileName.trimmed();

    // Sicherheitsprüfung: Falls nach der Bereinigung nichts mehr übrig ist
    if (fileName.isEmpty()) {
        fileName = "Internet-Link";
    }

    // --- NEU: Plattformabhängige Dateiendung festlegen ---
    QString extension = ".url";
#if defined(Q_OS_LINUX)
    extension = ".desktop";
#endif

    if (!fileName.endsWith(extension, Qt::CaseInsensitive)) {
        fileName += extension;
    }

    // 3. Zielverzeichnis prüfen
    QDir dir(targetDir);
    if (!dir.exists()) {
        qWarning() << "Zielverzeichnis existiert nicht:" << targetDir;
        return;
    }

    // 4. Kollisionsschutz: Falls die Datei schon existiert, eine Nummerierung anhängen
    // REPARATUR: QFileInfo nutzen statt hartem chop(4), da ".desktop" 8 Zeichen lang ist!
    QString fullPath = dir.filePath(fileName);
    QFileInfo fileInfo(fileName);
    QString baseName = fileInfo.completeBaseName();
    QString finalAnzeigename = baseName; // Merken für das 'Name=' Feld unter Linux

    if (QFile::exists(fullPath)) {
        int counter = 1;
        while (QFile::exists(fullPath)) {
            finalAnzeigename = QString("%1 (%2)").arg(baseName).arg(counter);
            QString numberedName = finalAnzeigename + extension;
            fullPath = dir.filePath(numberedName);
            counter++;
        }
    }

    // 5. Die Datei schreiben
    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);

#ifdef Q_OS_WIN
        // Windows .url Format
        out << "[InternetShortcut]\n";
        out << "URL=" << urlStr << "\n";
#elif defined(Q_OS_LINUX)
        // Linux .desktop Format für Web-Links
        out << "[Desktop Entry]\n";
        out << "Version=1.0\n";
        out << "Type=Link\n";
        out << "Name=" << finalAnzeigename << "\n"; // Das ist der Text, den Linux anzeigt
        out << "URL=" << urlStr << "\n";
        out << "Icon=text-html\n";                  // Standard-Icon für HTML/Web-Links
#endif

        file.close();
    } else {
        qCritical() << "Fehler beim Erstellen der Verknüpfung:" << file.errorString();
    }
}

bool isCurrentProcessElevated() {
#ifdef Q_OS_WIN
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hToken = NULL;
    bool elevated = false;

    // 1. Zugriff auf das Sicherheits-Token des Prozesses anfordern
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize = sizeof(elevation);

        // 2. Abfragen, ob das Token angehoben (elevated) ist
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            elevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return elevated;
#elif defined(Q_OS_LINUX)
    return (geteuid() == 0);
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
bool startProcessElevatedWin(const QString &programPath, const QString &arguments) {
    SHELLEXECUTEINFO shExecInfo = { 0 };
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS; // Erlaubt es uns, das Prozess-Handle zu tracken
    shExecInfo.hwnd = NULL;

    // "runas" ist der magische Schlüssel, der den UAC-Admin-Dialog erzwingt!
    shExecInfo.lpVerb = L"runas";

    // Qt-Strings in Windows-kompatible WCHAR-Arrays umwandeln (.utf16())
    shExecInfo.lpFile = reinterpret_cast<LPCWSTR>(programPath.utf16());
    shExecInfo.lpParameters = reinterpret_cast<LPCWSTR>(arguments.utf16());
    shExecInfo.lpDirectory = NULL;
    shExecInfo.nShow = SW_SHOWNORMAL;

    // Ausführen
    if (ShellExecuteEx(&shExecInfo)) {
        // Der Prozess wurde erfolgreich gestartet!
        // Da wir SEE_MASK_NOCLOSEPROCESS gesetzt haben, MÜSSEN wir das Handle schließen,
        // um Speicherlecks zu verhindern.
        if (shExecInfo.hProcess) {
            CloseHandle(shExecInfo.hProcess);
        }
        return true;
    }

    // Wenn wir hier landen, hat der Nutzer im UAC-Dialog auf "Nein" geklickt
    // oder die Datei wurde nicht gefunden (GetLastError() gibt Aufschluss).
    return false;
}


QString argumentsToWinString(const QStringList &args) {
    QStringList escapedArgs;

    for (QString arg : args) {
        // Falls das Argument Leerzeichen enthält oder leer ist, müssen wir es quoten
        if (arg.contains(' ') || arg.contains('\t') || arg.isEmpty()) {

            // Fiese Windows-Falle: Wenn das Argument auf einen Backslash endet,
            // würde das abschließende " durch den Backslash maskiert werden.
            // Lösung: Den letzten Backslash verdoppeln!
            if (arg.endsWith('\\')) {
                arg += '\\';
            }

            // In Anführungszeichen packen
            escapedArgs << QString("\"%1\"").arg(arg);
        } else {
            // Keine Leerzeichen? Einfach so übernehmen
            escapedArgs << arg;
        }
    }

    // Alle Argumente mit einem Leerzeichen dazwischen zusammenfügen
    return escapedArgs.join(' ');
}
#endif
