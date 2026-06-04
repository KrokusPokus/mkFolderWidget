#include "customtablemodel.h"
#include "helpers.h"

#include <QApplication>
#include <QDirIterator>
#include <QFontMetrics>
#include <QMimeData>
#include <QStorageInfo>

CustomTableModel::CustomTableModel(SettingsManager *settings, QObject *parent) : QAbstractTableModel(parent), m_settings(settings) {}

int CustomTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_files.size());
}

int CustomTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return 4;
}

QVariant CustomTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    int row = index.row();
    int col = index.column();

    if (row < 0 || row >= static_cast<int>(m_files.size())) {
        return QVariant();
    }

    const CustomFileInfo &file = m_files[row];

    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: return file.nameNoExt;
        case 1: {
            if (file.isDir) {
                return QVariant();
            }
            return formatAdaptiveSize(file.size);
        }
        case 2: return file.date.toString("yyyy-MM-dd  HH:mm:ss");
        case 3: return file.type;
        default: return QVariant();
        }
    }
    else if (role == Qt::EditRole) {
        switch (col) {
        case 0: return file.name;
        case 1: return file.size;
        case 2: return file.date;
        case 3: return file.type;
        default:
            return data(index, Qt::DisplayRole);
        }
    }
    else if (role == Qt::DecorationRole) {
        if (col != 0) return QVariant(); // Icons nur in der Namensspalte anzeigen

        QDir baseDir(m_currentDirectoryPath);
        QString absolutePath = baseDir.filePath(file.name);
#ifdef Q_OS_WIN
        if (!file.drivePath.isEmpty()) {
            absolutePath = file.drivePath;
        }
#endif
        if (m_thumbnailMode) {
            auto itThumb = m_thumbnailCache.find(absolutePath);
            if (itThumb != m_thumbnailCache.end()) {
                return itThumb.value(); // Liefert das fertige 96x96 Bild
            }
        }
        else {
            QHash<QString, QIcon>::iterator itPath = m_pathIconCache.find(absolutePath);
            if (itPath != m_pathIconCache.end()) {
                return itPath.value();
            }
        }

        // B) Wenn nicht, gehen wir über den Suffix- / Ordner-Cache
        if (file.isDir) {
            auto it = m_iconCache.find("//dir//");
            if (it == m_iconCache.end()) {
                it = m_iconCache.insert("//dir//", m_iconProvider.icon(QFileIconProvider::Folder));
            }
            return it.value();
        } else {
            int lastDot = file.name.lastIndexOf('.');
            QString suffix = (lastDot > 0) ? file.name.sliced(lastDot + 1).toLower() : "";

            auto it = m_iconCache.find(suffix);
            if (it == m_iconCache.end()) {
                QFileInfo dummyInfo("any_filename." + suffix);
                it = m_iconCache.insert(suffix, m_iconProvider.icon(dummyInfo));
            }
            return it.value();
        }
    }
    else if (role == UseRedTextRole) {
        // Prüfen, ob Datei ausführbar ist UND das Setting aktiv ist
        if (file.isExecutable && !file.isDir && m_settings->executableFilesRed) {
            return QVariant(true);
        }
        // Standardfarbe des Systems/Themes beibehalten
        return QVariant(false);
    }
    else if (role == CustomTableModel::IsCutRole) {
        return QVariant(file.isCut);
    }
    else if (role == CustomTableModel::IsDirectoryRole) {
        return QVariant(file.isDir);
    }
    else if (role == CustomTableModel::IsExecutableRole) {
        return QVariant(file.isExecutable);
    }
    else if (role == CustomTableModel::IsHiddenRole) {
        return QVariant(file.isHidden);
    }
    else if (role == CustomTableModel::FullFileInfoRole) {
        return QVariant::fromValue(file);
    }
    else if (role == Qt::TextAlignmentRole) {
        switch (col) {
        case 0:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case 2:
        case 3:
           return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    } else if (role == CustomTableModel::ListViewSizeHintRole) {
        return file.cachedSize;
    }

    return QVariant();
}

QVariant CustomTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return tr("Name");
        case 1:
            return tr("Size");
        case 2:
            return tr("Changed");
        case 3:
            return tr("Type");
        default:
            return QVariant();
        }
    }

    if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole) {
        switch (section) {
        case 0:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case 1:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case 2:
        case 3:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        default:
            return QVariant();
        }
    }

    // Für die vertikalen Header (Zeilennummern 1, 2, 3...) nutzen wir das Standardverhalten von Qt
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags CustomTableModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) {                             // No index -> no item -> background!
        return Qt::NoItemFlags | Qt::ItemIsDropEnabled;  // Background needs to have Qt::ItemIsDropEnabled to become a valid drop target.
    }

    // Die Standard-Flags holen (Selectable, Enabled)
    Qt::ItemFlags cellFlags = QAbstractTableModel::flags(index);

    if (index.column() == 0) {
        cellFlags |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;

        // Nur Ordner dürfen als Drop-Ziel dienen
        int row = index.row();
        if (row >= 0 && row < static_cast<int>(m_files.size())) {
            if (m_files[row].isDrive) {
                cellFlags &= ~Qt::ItemIsEditable;
            }
            if (m_files[row].isDir) {
                cellFlags |= Qt::ItemIsDropEnabled;
            }
        }
    }

    return cellFlags;
}

bool CustomTableModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || role != Qt::EditRole || index.column() != 0) {
        return false;
    }

    int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_files.size())) {
        return false;
    }

    QString newName = cleanFileName(value.toString().trimmed());

    // Abbrechen, wenn der Name leer ist oder sich gar nicht geändert hat
    if (newName.isEmpty() || newName == m_files[row].name) {
        return false;
    }

    // --- 1. DATEI AUF DER FESTPLATTE UMBENENNEN ---
    QDir dir(m_currentDirectoryPath);
    QString oldPath = dir.filePath(m_files[row].name);
    QString newPath = dir.filePath(newName);

    // Versuchen, das Filesystem-Objekt umzubenennen
    if (!QFile::rename(oldPath, newPath)) {
        // Wenn das OS streikt (z.B. Datei geöffnet, keine Rechte), brechen wir ab
        return false;
    }


    // 2. Struct im Speicher aktualisieren
    CustomFileInfo &info = m_files[row];

    QString ext;
    info.name = newName;
    info.nameNoExt = newName;

    if (!info.isDir) {
        int lastDot = newName.lastIndexOf('.');
        if (lastDot > 0) {
            if (!m_settings->showFileExtensions) {
                info.nameNoExt = newName.sliced(0, lastDot);
            }
            ext = info.name.sliced(lastDot + 1).toLower(); // falls sich der Typ geändert hat
        }
    }

    info.type = ext;

#ifdef Q_OS_WIN
    info.isExecutable = (ext == "exe" || ext == "scr");
#endif

    // --- 3. VIEW BENACHRICHTIGEN ---
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

    return true;
}

void CustomTableModel::sort(int column, Qt::SortOrder order) {
    // We're not using any of it. The Proxy-Modell will take over this job.
    Q_UNUSED(column);
    Q_UNUSED(order);
}

// Daten neu setzen und der View signalisieren, dass sie sich neu zeichnen muss
void CustomTableModel::setFiles(const std::vector<CustomFileInfo> &files) {
    beginResetModel();
    m_files = files;
    endResetModel();
}

void CustomTableModel::loadDirectory(const QString &dirPath) {
    std::vector<CustomFileInfo> newFiles;
    m_currentDirectoryPath = dirPath;

    // Einmalig die FontMetrics der ListView holen

    QFontMetrics fm(QApplication::font());
    const int iconWidth = 16;
    const int padding = 20;   // Abstand links, rechts, zwischen Icon und Text
    const int rowHeight = 18; // Feste Höhe für Listenmodus (oder dynamisch)

#ifdef Q_OS_WIN
    if (m_currentDirectoryPath == "drives://") {
        QFileInfoList drives = QDir::drives();
        for (const QFileInfo &driveInfo : std::as_const(drives)) {
            CustomFileInfo info;
            info.drivePath = driveInfo.absoluteFilePath();

            QStorageInfo storage(driveInfo.absoluteFilePath());
            QString volumeName = storage.name();

            // Fallback: Falls das Laufwerk keinen Namen hat (oder z.B. ein leeres CD-Laufwerk ist)
            if (volumeName.isEmpty()) {
                if (storage.fileSystemType() == "CDFS" || storage.fileSystemType() == "UDF") {
                    volumeName = tr("Optical Drive");
                } else {
                    volumeName = tr("Local Drive");
                }
            }

            QString driveLetter = driveInfo.absoluteFilePath().left(2);

            info.name = QString("(%1) %2").arg(driveLetter, volumeName);

            info.isDir = true;
            info.isDrive = true;
            info.isHidden = false;
            info.size = storage.bytesTotal();
            info.date = QDateTime();
            info.iconIndex = 0;
            info.nameNoExt = info.name;
            info.type = storage.fileSystemType();
            info.isExecutable = false;

            int textWidth = fm.horizontalAdvance(info.nameNoExt);
            int totalWidth = textWidth + iconWidth + padding;
            info.cachedSize = QSize(totalWidth, rowHeight);

            m_pathIconCache.insert(info.drivePath, m_iconProvider.icon(driveInfo));

            newFiles.push_back(info);
        }
    }
    else
#endif
        {
        QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot;
        QDirIterator it(dirPath, filters, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            it.next();
    
            QFileInfo fileInfo = it.fileInfo();
    
            CustomFileInfo info;
            info.name = fileInfo.fileName();
            info.isDir = fileInfo.isDir();
#ifdef Q_OS_WIN
            info.isHidden = fileInfo.isHidden() || info.name.startsWith('.');
#else
            info.isHidden = fileInfo.isHidden();
#endif
            info.size = info.isDir ? 0 : fileInfo.size();
            info.date = fileInfo.lastModified();
            info.iconIndex = 0;
    
            QString ext;
            info.nameNoExt = info.name;
    
            if (!info.isDir) {
                int lastDot = info.name.lastIndexOf('.');
                if (lastDot > 0) {
                    if (!m_settings->showFileExtensions) {
                        info.nameNoExt = info.name.sliced(0, lastDot);
                    }
                    ext = info.name.sliced(lastDot + 1).toLower();
                }
            }
    
            info.type = ext;
    
#ifdef Q_OS_WIN
            info.isExecutable = (ext == "exe" || ext == "scr");
#else
            info.isExecutable = fileInfo.isExecutable();
#endif
            int textWidth = fm.horizontalAdvance(info.nameNoExt);
            int totalWidth = textWidth + iconWidth + padding;
            info.cachedSize = QSize(totalWidth, rowHeight);

            newFiles.push_back(info);
        }
    }

    beginResetModel();
    m_files = std::move(newFiles);
    endResetModel();
}

QString CustomTableModel::filePath(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this) {
        return QString();
    }

    int row = index.row();

    if (row < 0 || row >= static_cast<int>(m_files.size())) {
        return QString();
    }

    const CustomFileInfo &file = m_files[row];

#ifdef Q_OS_WIN
    if (m_currentDirectoryPath == "drives://") {
        return file.drivePath;
    } else
#endif
    {
        QDir baseDir(m_currentDirectoryPath);
        return baseDir.filePath(file.name);
    }
}

bool CustomTableModel::hasPathIcon(const QString &path) const {
    return m_pathIconCache.contains(path);
}

void CustomTableModel::addPathIcon(const QString &path, const QIcon &icon, int row) {
    m_pathIconCache.insert(path, icon);

    // Sofort die View benachrichtigen, dass genau DIESE Zeile (Spalte 0) neu gezeichnet werden muss
    QModelIndex idx = this->index(row, 0);
    emit dataChanged(idx, idx, {Qt::DecorationRole});
}

bool CustomTableModel::hasPathThumbnail(const QString &path) const {
    return m_thumbnailCache.contains(path);
}

void CustomTableModel::addPathThumbnail(const QString &path, const QPixmap &thumb, int row) {
    m_thumbnailCache.insert(path, thumb);

    // Sofort die View benachrichtigen, dass genau DIESE Zeile (Spalte 0) neu gezeichnet werden muss
    QModelIndex idx = this->index(row, 0);
    emit dataChanged(idx, idx, {Qt::DecorationRole});
}

void CustomTableModel::setCutMarkers(const QStringList &absolutePaths) {
    // Wir wandeln die Liste zur performanten Suche in ein QSet um
    QSet<QString> cutSet(absolutePaths.begin(), absolutePaths.end());
    QDir baseDir(m_currentDirectoryPath);

    for (size_t i = 0; i < m_files.size(); ++i) {
        QString fullPath = baseDir.filePath(m_files[i].name);
        bool newCutState = cutSet.contains(fullPath);

        if (m_files[i].isCut != newCutState) {
            m_files[i].isCut = newCutState;

            QModelIndex startIdx = this->index(static_cast<int>(i), 0);
            QModelIndex endIdx = this->index(static_cast<int>(i), this->columnCount() - 1);
            emit dataChanged(startIdx, endIdx, {CustomTableModel::IsCutRole, Qt::DecorationRole});
        }
    }
}

void CustomTableModel::clearCutMarkers() {
    for (size_t i = 0; i < m_files.size(); ++i) {
        if (m_files[i].isCut) {
            m_files[i].isCut = false;

            QModelIndex startIdx = this->index(static_cast<int>(i), 0);
            QModelIndex endIdx = this->index(static_cast<int>(i), this->columnCount() - 1);
            emit dataChanged(startIdx, endIdx, {CustomTableModel::IsCutRole, Qt::DecorationRole});
        }
    }
}

bool CustomTableModel::isDirectory(int row) const {
    if (row < 0 || row >= static_cast<int>(m_files.size())) {
        return false;
    }

    return m_files[row].isDir;
}

Qt::DropActions CustomTableModel::supportedDragActions() const {
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions CustomTableModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

QStringList CustomTableModel::mimeTypes() const {
    QStringList types;
    types << "text/uri-list"; // Der Standard-MIME-Type für Dateien im OS
    return types;
}

QMimeData* CustomTableModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = new QMimeData();

    QList<QUrl> urls;
    QSet<int> processedRows; // Verhindert Duplikate bei Mehrfachauswahl von Spalten

    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            processedRows.insert(index.row());
        }
    }

    QDir baseDir(m_currentDirectoryPath);
    for (int row : processedRows) {
        if (row >= 0 && row < static_cast<int>(m_files.size())) {
            QString absolutePath = baseDir.filePath(m_files[row].name);
            urls.append(QUrl::fromLocalFile(absolutePath));
        }
    }

    mimeData->setUrls(urls);

    return mimeData;
}

bool CustomTableModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const {
    // Basis-Check: Wenn keine URLs im Drag stecken, direkt verbieten
    if (!data || !data->hasUrls()) {
        return false;
    }

    // Drop "zwischen" Items verbieten!
    // Wenn row != -1 ist, schwebt die Maus an den Rändern eines Items (oberhalb/unterhalb).
    // Durch das 'false' blendet Qt die Einfüge-Linie aus und wechselt sofort in den "OnItem"-Modus.
    if (row != -1) {
        return false;
    }

    // FALL 1: Die Maus schwebt über dem leeren Hintergrund (parent ist ungültig)
    if (!parent.isValid()) {
        return true;
    }

    // FALL 2: Maus schwebt über einem konkreten Item (z. B. einem Ordner)
    int parentRow = parent.row();
    if (parentRow >= 0 && parentRow < static_cast<int>(m_files.size())) {

        // Wir prüfen das Ganze nur, wenn das Ziel-Item ein Ordner ist
        if (m_files[parentRow].isDir) {
            QDir currentDir(m_currentDirectoryPath);
            // Das ist der absolute Pfad des Ordners, über dem die Maus gerade schwebt:
            QString targetFolderPath = currentDir.absoluteFilePath(m_files[parentRow].name);

            // Schleife über alle Dateien/Ordner, am Mauszeiger hängen
            QList<QUrl> urls = data->urls();
            for (const QUrl &url : std::as_const(urls)) {
                QString draggedPath = url.toLocalFile();

                // SCHUTZ 1: Ordner auf sich selbst droppen (Identischer Pfad)
                // Verhindert z.B. das Droppen von /Projekte/A auf /Projekte/A in Instanz 2
                if (draggedPath == targetFolderPath) {
                    return false; // Sofort blockieren! Zeigt das Verbots-Schild.
                }

                // SCHUTZ 2: Ordner in einen eigenen Unterordner droppen
                // Verhindert das physikalisch unmögliche Verschieben von /A in /A/B/C
                if (targetFolderPath.startsWith(draggedPath + "/")) {
                    return false; // Ebenfalls blockieren!
                }
            }
        }
    }
    return QAbstractTableModel::canDropMimeData(data, action, row, column, parent);
}

bool CustomTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    Q_UNUSED(row);
    Q_UNUSED(column);
/*
    // --- DEBUG-BLOCK START ---
    if (data) {
        qDebug() << "\n========== QMimeData INSIGHT ==========";
        qDebug() << "Verfügbare Formate:" << data->formats();

        for (const QString &format : data->formats()) {
            QByteArray rawData = data->data(format);
            qDebug() << "---------------------------------------";
            qDebug() << "MIME-Type:" << format;
            qDebug() << "Größe:    " << rawData.size() << "Bytes";

            // Spezialbehandlung für URLs (sehr nützlich bei Browser-Dnd)
            if (format == "text/uri-list" && data->hasUrls()) {
                qDebug() << "Inhalt (als URLs interpretiert):";
                for (const QUrl &url : data->urls()) {
                    qDebug() << "  -> URL:" << url.toString()
                    << "[Lokale Datei?" << (url.isLocalFile() ? "JA" : "NEIN") << "]";
                }
            }
            // Reine Text-Formate lesbar als String ausgeben
            else if (format.startsWith("text/") || format.contains("string") || format.contains("text")) {
                // Konvertiert die Rohdaten von UTF-8 in einen lesbaren QString
                QString textContent = QString::fromUtf8(rawData);
                // Zeilenumbrüche für die Ausgabe etwas einrücken
                textContent.replace("\n", "\n  ");
                qDebug() << "Inhalt (Text):\n  " << textContent;
            }
            // Binäre oder Qt-interne Formate als Hex-Vorschau anzeigen
            else {
                // Wir zeigen nur die ersten 64 Bytes an, damit das Terminal nicht geflutet wird
                QByteArray preview = rawData.left(64);
                qDebug() << "Inhalt (Hex-Vorschau):\n  " << preview.toHex(' ');
                if (rawData.size() > 64) {
                    qDebug() << "  ... (" << (rawData.size() - 64) << "weitere Bytes)";
                }
            }
        }
        qDebug() << "=======================================\n";
    } else {
        qDebug() << "dropMimeData aufgerufen, aber QMimeData ist NULL!";
    }
    // --- DEBUG-BLOCK ENDE ---
*/

    if (action == Qt::IgnoreAction) return true;
    if (!data || !data->hasUrls()) return false;

    // Standardziel: Das aktuell geöffnete Verzeichnis
    QString targetDir = m_currentDirectoryPath;

    // Wenn auf ein gültiges Item gedroppt wurde und das ein Ordner ist,
    // verschieben/kopieren wir die Dateien IN diesen Unterordner!
    if (parent.isValid()) {
        int parentRow = parent.row();
        if (parentRow >= 0 && parentRow < static_cast<int>(m_files.size()) && m_files[parentRow].isDir) {
            QDir dir(m_currentDirectoryPath);
            targetDir = dir.filePath(m_files[parentRow].name);
        }
    }

    QList<QUrl> urlList = data->urls();
    if (!urlList.first().isLocalFile()) {
        for (const QUrl &url : data->urls()) {
            if (!url.isLocalFile()) {
                QString webUrlStr = url.toString();

                QString webTitle = "";

                if (data->hasFormat("text/x-moz-url-desc")) {
                    QByteArray rawDesc = data->data("text/x-moz-url-desc");

                    // Firefox liefert das unter Windows als UTF-16 (2 Bytes pro Zeichen).
                    // Wir wandeln die Rohdaten explizit von UTF-16 in einen QString um:
                    webTitle = QString::fromUtf16(
                        reinterpret_cast<const char16_t*>(rawDesc.constData()),
                        rawDesc.size() / 2
                        );

                    // Da Windows-Strings oft mit einem Null-Terminator '\0' enden,
                    // schneiden wir diesen und eventuelle Leerzeichen ab.
                    int nullPos = webTitle.indexOf(QChar('\0'));
                    if (nullPos != -1) {
                        webTitle = webTitle.left(nullPos);
                    }
                    webTitle = webTitle.trimmed();
                }

                // Fallback, falls es kein Firefox/Mozilla-Browser war oder das Feld fehlt
                if (webTitle.isEmpty() && data->hasText()) {
                    // Wenn es eine URL ist, extrahieren wir den Hostnamen, sonst nehmen wir den Text
                    QUrl url(data->text());
                    webTitle = url.isValid() && !url.isLocalFile() ? url.host() : data->text();
                }

                createInternetShortcut(webUrlStr, targetDir, webTitle);
            }
        }

        return true;
    }

    emit filesDropped(data->urls(), targetDir, action);

    return true;
}
