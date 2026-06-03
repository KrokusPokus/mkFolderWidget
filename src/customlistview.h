#ifndef CUSTOMLISTVIEW_H
#define CUSTOMLISTVIEW_H

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QListView>



class CustomListView : public QListView {
    Q_OBJECT

signals:
    void filesDropped(const QList<QUrl> &urls, const QString &targetDirectory, Qt::DropAction action);

public:
    explicit CustomListView(QWidget *parent = nullptr);

private:
    void updateTargetCache(const QPoint &pos, const QMimeData *mimeData);
    Qt::DropAction resolveDropAction(Qt::KeyboardModifiers mods) const;
    bool interceptInvalidFileTarget(QDragMoveEvent *event, bool isEnterEvent);

    bool m_hadMultiSelectionBeforeClick = false;

    QString m_targetDirPrevious;
    bool m_isSameFolderCached = false;
    bool m_isSameDriveCached = false;

    QModelIndex m_selectionAnchor;  // Part of mitigation for Shift+Up/Shift+Down range selection bug. Not needed in tableView



protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) override;
};

#endif // CUSTOMLISTVIEW_H
