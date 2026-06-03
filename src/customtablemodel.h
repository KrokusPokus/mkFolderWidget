#ifndef CUSTOMTABLEMODEL_H
#define CUSTOMTABLEMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QUrl>
#include <vector>

#include "settingsmanager.h"

struct CustomFileInfo {
    QString name;
    QString nameNoExt;
    qint64 size;
    QDateTime date;
    QString type;
    int iconIndex;
    bool isCut = false;
    bool isDir;
    bool isExecutable;
    bool isHidden;
};

Q_DECLARE_METATYPE(CustomFileInfo)

class CustomTableModel : public QAbstractTableModel {
    Q_OBJECT

signals:
    void filesDropped(const QList<QUrl> &urls, const QString &targetDirectory, Qt::DropAction action);

public:
    enum ModelRoles {
        IsCutRole = Qt::UserRole + 1,
        IsDirectoryRole = Qt::UserRole + 2,
        IsExecutableRole = Qt::UserRole + 3,
        IsHiddenRole = Qt::UserRole + 4,
        UseRedTextRole = Qt::UserRole + 5,
        FullFileInfoRole = Qt::UserRole + 6
    };

    explicit CustomTableModel(SettingsManager *settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // Drag&Drop related
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;                  // Gibt an, welche Aktionen (Kopieren/Verschieben) erlaubt sind
    QStringList mimeTypes() const override;                                 // Sagt dem System, dass wir mit Datei-Pfaden arbeiten
    QMimeData* mimeData(const QModelIndexList &indexes) const override;     // Packt die ausgewählten Dateien beim Draggen in das Clipboard/Mime-Objekt
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;           // Verarbeitet das Loslassen (Drop) der Dateien
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;  // Verarbeitet das Mauszeiger-Feedback während dem Ziehen

    // Funktion, um später deine eingelesenen Daten zu übergeben
    void setFiles(const std::vector<CustomFileInfo> &files);

    void loadDirectory(const QString &dirPath);
    QString filePath(const QModelIndex &index) const;

    bool hasPathIcon(const QString &path) const;
    void addPathIcon(const QString &path, const QIcon &icon, int row);

    bool hasPathThumbnail(const QString &path) const;
    void addPathThumbnail(const QString &path, const QPixmap &thumb, int row);

    void setCutMarkers(const QStringList &absolutePaths);
    void clearCutMarkers();
    bool isDirectory(int row) const;
    const QString &currentDirectoryPath() const { return m_currentDirectoryPath; }

    void setThumbnailMode(bool enabled) {
        m_thumbnailMode = enabled;
    }

private:
    std::vector<CustomFileInfo> m_files;
    QString m_currentDirectoryPath;
    bool m_thumbnailMode = false;

    QFileIconProvider m_iconProvider;
    mutable QHash<QString, QIcon> m_iconCache;      // Cache für Suffixe (z.B. "txt", "pdf")
    mutable QHash<QString, QIcon> m_pathIconCache;  // Cache für spezifische Pfade (z.B. exe-Dateien)
    mutable QHash<QString, QPixmap> m_thumbnailCache;

    SettingsManager *m_settings;
};



#endif // CUSTOMTABLEMODEL_H
