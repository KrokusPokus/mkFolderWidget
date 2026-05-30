#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "customlistview.h"
#include "custommodel.h"
#include "customtableview.h"
#include "settingsmanager.h"
#include "filesortproxymodel.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QElapsedTimer>
#include <QFileIconProvider>
#include <QFileSystemWatcher>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QPainter>             // Added for cut item opacity drawing
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyledItemDelegate>  // Added for cut item opacity drawing
#include <QTableView>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>


class ListItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);

        if (index.data(CustomTableModel::UseRedTextRole).toBool()) {
            static const QColor exeRed(255, 74, 70);
            option->palette.setColor(QPalette::Text, exeRed);
            option->palette.setColor(QPalette::HighlightedText, exeRed);
        }
    }

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        bool isCut = index.data(CustomTableModel::IsCutRole).toBool();
        bool isHidden = index.data(CustomTableModel::IsHiddenRole).toBool();

        QStyleOptionViewItem opt = option;
        opt.decorationPosition = QStyleOptionViewItem::Left;
        opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;

        /*
        // Eigenen Selektions- und Fokus-Hintergrund zeichnen
        bool isSelected = (opt.state & QStyle::State_Selected);
        bool hasFocus   = (opt.state & QStyle::State_HasFocus);
        if (isSelected || hasFocus) {
            QRectF rect = opt.rect;
            rect.adjust(1, 1, -1, -1);

            if (isSelected) {
                //QColor bgColor = QColor(255, 0, 255, 0);
                QColor bgColor = opt.palette.color(QPalette::Highlight);
                bgColor.setAlpha(hasFocus ? 100 : 100);
                painter->setBrush(bgColor);
            } else {
                painter->setBrush(Qt::NoBrush);
            }

            if (hasFocus) {
                QColor borderColor = opt.palette.color(QPalette::Highlight);
                painter->setPen(QPen(borderColor, 1, Qt::SolidLine));
            } else {
                painter->setPen(Qt::NoPen);
            }

            painter->drawRect(rect);
        }

        // 3. Das restliche Standard-Zeichnen modifizieren
        opt.state &= ~QStyle::State_HasFocus; // Standard-Fokusrahmen ausschalten
        opt.state &= ~QStyle::State_Selected; // Standard-Auswahlbalken ausschalten
        if (isSelected) {
            opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::WindowText));
        }
        */
        if (isCut || isHidden) {
            painter->save();
            if (isCut && isHidden) {
                painter->setOpacity(0.25);
            } else {
                painter->setOpacity(0.50);
            }
            QStyledItemDelegate::paint(painter, opt, index);
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, opt, index);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        opt.decorationPosition = QStyleOptionViewItem::Left;
        return QStyledItemDelegate::sizeHint(opt, index);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,  const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;

        opt.decorationPosition = QStyleOptionViewItem::Left;
        opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;

        QStyledItemDelegate::updateEditorGeometry(editor, opt, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        QStyledItemDelegate::setEditorData(editor, index);

        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            QString text = lineEdit->text();

            int lastDot = text.lastIndexOf('.');
            bool isDir = index.data(CustomTableModel::IsDirectoryRole).toBool();

            if (!isDir && lastDot > 0) {
                QTimer::singleShot(0, lineEdit, [lineEdit, lastDot]() {
                    lineEdit->setSelection(0, lastDot);
                });
            } else {
                lineEdit->selectAll();
            }
        }
    }
};

class TableItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);

        if (index.data(CustomTableModel::UseRedTextRole).toBool()) {
            static const QColor exeRed(255, 74, 70);
            option->palette.setColor(QPalette::Text, exeRed);
            option->palette.setColor(QPalette::HighlightedText, exeRed);
        }
    }

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        bool isCut = index.data(CustomTableModel::IsCutRole).toBool();
        bool isHidden = index.data(CustomTableModel::IsHiddenRole).toBool();

        if (isCut || isHidden) {
            painter->save();
            if (isCut && isHidden) {
                painter->setOpacity(0.25);
            } else {
                painter->setOpacity(0.50);
            }
            QStyledItemDelegate::paint(painter, option, index);
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

class ThumbItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);

        option->displayAlignment = Qt::AlignHCenter | Qt::AlignBottom;

        if (index.data(CustomTableModel::UseRedTextRole).toBool()) {
            static const QColor exeRed(255, 74, 70);
            option->palette.setColor(QPalette::Text, exeRed);
            option->palette.setColor(QPalette::HighlightedText, exeRed);
        }
    }

public:
    using QStyledItemDelegate::QStyledItemDelegate;
/*
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {

        bool isCut = index.data(CustomTableModel::IsCutRole).toBool();
        bool isHidden = index.data(CustomTableModel::IsHiddenRole).toBool();

        if (isCut || isHidden) {
            painter->save();
            if (isCut && isHidden) {
                painter->setOpacity(0.25);
            } else {
                painter->setOpacity(0.50);
            }
            QStyledItemDelegate::paint(painter, option, index);
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
*/
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        // Feste Icon-Box berechnen (Nutzt die 96px aus der View)
        int iconBoxSize = option.decorationSize.isValid() ? option.decorationSize.height() : 96;

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        bool isCut = index.data(CustomTableModel::IsCutRole).toBool();
        bool isHidden = index.data(CustomTableModel::IsHiddenRole).toBool();

        painter->save();

        // =====================================================================
        // Deine Deckkraft-Logik steuert jetzt das gesamte Custom-Rendering
        // =====================================================================
        if (isCut || isHidden) {
            if (isCut && isHidden) {
                painter->setOpacity(0.25);
            } else {
                painter->setOpacity(0.50);
            }
        }

        // 2. Hintergrund zeichnen (Hover, Selektion, Fokus-Rechteck)
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        QRect itemRect = opt.rect;
        qDebug() << "itemRect.left()" << itemRect.left() << "itemRect.top()" << itemRect.top() << "itemRect.right()" << itemRect.right() << "itemRect.bottom()" << itemRect.bottom();

        QRect iconBoxRect(itemRect.left() + (itemRect.width() - iconBoxSize) / 2,
                          itemRect.top(),
                          iconBoxSize,
                          iconBoxSize);

        // Icon innerhalb der festen 96x96 Box zentrieren und zeichnen
        if (!opt.icon.isNull()) {
            QSize actualSize = opt.icon.actualSize(QSize(iconBoxSize, iconBoxSize), (opt.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
            QRect targetIconRect = QStyle::alignedRect(opt.direction, Qt::AlignCenter, actualSize, iconBoxRect);
            opt.icon.paint(painter, targetIconRect, Qt::AlignCenter, (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled);
        }

        // 4. Text-Box berechnen (strikt unterhalb der 96px Icon-Box)
        QRect textRect = itemRect;
        textRect.setTop(iconBoxRect.bottom() + 4); // 4px Abstand halten
        QRect elideRect = textRect.adjusted(4, 0, -4, 0);

        if (!opt.text.isEmpty()) {
            painter->setFont(opt.font);

            // Textfarbe anhand des Selektionsstatus ermitteln
            QPalette::ColorGroup cg = (opt.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
            if (opt.state & QStyle::State_Selected) {
                painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
            } else {
                painter->setPen(opt.palette.color(cg, QPalette::Text));
            }

            QString elidedText = opt.fontMetrics.elidedText(
                opt.text,
                Qt::ElideMiddle,  // Kürzen am Ende (z.B. "SehrLangerName...")
                elideRect.width() // Verfügbare Breite im Widget
                );

            painter->drawText(elideRect, opt.displayAlignment, elidedText);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        // 1. Basis-Größe vom Standard-Kalkulator holen
        QSize size = QStyledItemDelegate::sizeHint(option, index);

        // 2. Mindesthöhe berechnen: Icon-Höhe (decorationSize) + Text-Höhe + Padding
        // option.decorationSize enthält die 96px, die du im View gesetzt hast
        int minHeight = option.decorationSize.height() + option.fontMetrics.height() + 8;

        // 3. Falls Qt (z.B. im Hintergrund) fälschlicherweise eine zu kleine Höhe berechnet
        if (size.height() < minHeight) {
            size.setHeight(minHeight);
        }

        // Sicherstellen, dass auch die Breite dem Grid entspricht
        if (size.width() < 120) {
            size.setWidth(120);
        }

        return size;
    }
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &targetDirectoryr = QString(), const QString &focusPath = QString(), QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    Q_SCRIPTABLE void openPath(const QString &path);

private slots:
    void onClipboardChanged();
    void onDirectoryChangedOnDisk(const QString &path);
    void onListItemDoubleClicked(const QModelIndex &index);
    void onListViewHeaderClicked();
    void onTimedUpdateIcons();
    void onVerticalBarScrollChange();
    void onHorizontalBarScrollChange();
    void setViewMode(int index);
    void triggerDirectoryReload();
    void onFilterBoxChange(const QString &text);

private:
    enum Column {
        eColName = 0,
        eColSize = 1,
        eColDate = 2,
        eColType = 3
    };

    QString getActiveViewCurrentItemPath();
    QStringList getActiveViewPathList();
    QSet<int> getActiveViewRowSet();


    bool showDeleteConfirmationDialog(const QStringList &pathList, bool bRecycleOnly);
    void action_EditSettingsFile();
    void action_ListViewCopyFiles();
    void action_ListViewCopyPaths();
    void action_ListViewCutFiles();
    void action_ListViewDeleteFiles(bool bRecycleOnly);
    void action_ListViewEditFiles();
    void action_ListViewFileProperties();
    void action_ListViewOpenFiles();
    void action_ListViewPasteFiles();
    void action_ListViewRenameFiles();
    void action_ListViewNewFolder();
    void action_ListViewNewTextFile();
    void browseFolder(QString directoryPath, const QString &focusPath = QString(), bool isHistoryNavigation = false);
    void loadMimeCache();
    void navigateUp();
    void navigateBack();
    void navigateForward();
    void onFilesDropped(const QList<QUrl> &urls, const QString &targetDir, Qt::DropAction dropAction, bool fromClipboard = false);
    void onShowContextMenu(QAbstractItemView *senderView, const QPoint &pos);
    void parseMimeAppsList(const QString &path);
    void parseMimeInfoCache(const QString &path);
    void removeCutMarkers();
    void selectAllItems();
    void setupClipboardForCopyOrCut(const QStringList &cutFilePaths, bool isCut);
    void updateColumns();

    CustomTableModel *m_abstractModel = nullptr;
    QItemSelectionModel *m_selectionModel = nullptr;
    FileSortProxyModel *m_proxyModel = nullptr;

    QWidget *m_centralWidget = nullptr;
    QVBoxLayout *m_mainLayout = nullptr;
    QWidget *m_topControlsContainerWidget = nullptr;

    QLineEdit *m_LineEdit1 = nullptr;
    CustomTableView *m_tableView = nullptr;
    CustomListView *m_listView = nullptr;
    CustomListView *m_thumbnailView = nullptr;
    QStackedWidget *m_viewStack = nullptr;
    QAction *m_actionListViewOpenFiles = nullptr;
    QAction *m_actionListViewEditFiles = nullptr;
    QAction *m_actionListViewCopyPaths = nullptr;
    QAction *m_actionListViewCutFiles = nullptr;
    QAction *m_actionListViewCopyFiles = nullptr;
    QAction *m_actionListViewDeleteFiles = nullptr;
    QAction *m_actionListViewRenameFiles = nullptr;
    QAction *m_actionListViewFileProperties = nullptr;
    QAction *m_actionListViewPasteFiles = nullptr;
    QAction *m_actionListViewNewFolder = nullptr;
    QAction *m_actionListViewNewTextFile = nullptr;
    QThread *m_workerThread = nullptr;
    QTimer *m_timerUpdateIcons = nullptr;

    QFileSystemWatcher* m_fileSystemWatcher = nullptr;
    QTimer *m_watcherDebounceTimer = nullptr;
    bool m_ignoreNextWatcherSignal = false; // Flag für eigene Datei-Aktionen

    bool m_bShowHiddenFiles = false;
    QString m_currentDirectory;
    QString m_activeFilterTerms;
    QElapsedTimer m_BenchmarkTimer;
    QFileIconProvider m_iconProvider;
    QHash<QString, QStringList> m_mimeCache;
    QByteArray m_currentClipboardToken;
    SettingsManager m_settings;
    QStringList m_backHistory;
    QStringList m_forwardHistory;

#ifdef Q_OS_WIN
    QString getSendToPath();
#endif

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
};
#endif // MAINWINDOW_H

