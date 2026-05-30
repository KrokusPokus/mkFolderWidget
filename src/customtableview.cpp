#include "customtableview.h"
#include <QDrag>
#include <QMenu>
#include <QMimeData>

CustomTableView::CustomTableView(QWidget *parent)
    : QTableView(parent)
{
    // Hier könntest du auch Standard-Einstellungen für deine View setzen,
    // z.B. setAcceptDrops(true);
}

void CustomTableView::startDrag(Qt::DropActions supportedActions) {
    // 1. Prüfen, ob die rechte Maustaste gedrückt ist, während der Drag startet
    bool isRightClick = (QGuiApplication::mouseButtons() & Qt::RightButton);

    // 2. Standard-MimeData vom Model für die ausgewählten Elemente generieren lassen
    QMimeData *mimeData = model()->mimeData(selectedIndexes());
    if (!mimeData) return;

    // 3. Wenn es ein Rechtsklick-Drag ist, flaggen wir das Objekt temporär
    if (isRightClick) {
        mimeData->setData("application/x-rightclickdrag", QByteArray("1"));
    }

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // Optional: Hier könntest du noch ein passendes Drag-Bild (Pixmap) setzen
    // drag->setPixmap(...);

    // 4. Drag ausführen und explizit alle drei Aktionen erlauben
    if (isRightClick) {
        drag->exec(supportedActions | Qt::CopyAction | Qt::MoveAction | Qt::LinkAction);
    } else {
        drag->exec(supportedActions | Qt::CopyAction | Qt::MoveAction);
    }
}

void CustomTableView::dragMoveEvent(QDragMoveEvent *event) {
    QModelIndex index = indexAt(event->position().toPoint());
    if (!index.isValid()) { // empty background area
        // Wenn die Quelle aus uns selbst stammt (interner Drag), verbieten wir es SOFORT
        if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") && event->dropAction() == Qt::MoveAction) {
            event->ignore();
            return;
        }
    }

    QTableView::dragMoveEvent(event);

    // Override the DropAction chosen by the default handler
    if (event->isAccepted()) {
        if (event->modifiers() & Qt::ControlModifier) {
            event->setDropAction(Qt::CopyAction);
        } else {
            event->setDropAction(Qt::MoveAction);
        }
        event->accept();
    }
}

void CustomTableView::dropEvent(QDropEvent *event) {
    bool isRightClick = event->mimeData()->hasFormat("application/x-rightclickdrag");
    if (isRightClick) {
        QMenu menu(this);
        QAction *actCopy = menu.addAction(tr("Copy here"));
        QAction *actMove = menu.addAction(tr("Move here"));
        QAction *actLink = menu.addAction(tr("Link here"));
        menu.addSeparator();
        menu.addAction(tr("Cancel"));

        menu.setDefaultAction(actMove);

        QAction *selectedAction = menu.exec(mapToGlobal(event->position().toPoint()));

        if (selectedAction == actCopy) {
            event->setDropAction(Qt::CopyAction);
        } else if (selectedAction == actMove) {
            event->setDropAction(Qt::MoveAction);
        } else if (selectedAction == actLink) {
            event->setDropAction(Qt::LinkAction);
        } else {
            event->ignore();
            return;
        }
    } else {
        if (event->modifiers() & Qt::ControlModifier) {
            event->setDropAction(Qt::CopyAction);
        } else {
            event->setDropAction(Qt::MoveAction);
        }
    }

    CustomTableView::dropEvent(event);
}

void CustomTableView::mousePressEvent(QMouseEvent *event) {
    if (selectionModel()) {
        QModelIndexList selectedIdxList = selectionModel()->selectedIndexes();
        QSet<int> uniqueRows;
        for (const QModelIndex &idx : std::as_const(selectedIdxList)) {
            uniqueRows.insert(idx.row());
        }
        m_hadMultiSelectionBeforeClick = (uniqueRows.size() > 1);
    } else {
        m_hadMultiSelectionBeforeClick = false;
    }

    QTableView::mousePressEvent(event);
}

bool CustomTableView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) {
    if (trigger == QAbstractItemView::SelectedClicked && m_hadMultiSelectionBeforeClick) {
        return false;
    }

    return QTableView::edit(index, trigger, event);
}
