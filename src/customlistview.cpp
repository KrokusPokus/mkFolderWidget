#include "customlistview.h"
#include <QDrag>
#include <QMenu>
#include <QMimeData>

CustomListView::CustomListView(QWidget *parent)
    : QListView(parent)
{
    // Hier könntest du auch Standard-Einstellungen für deine View setzen,
    // z.B. setAcceptDrops(true);
}

void CustomListView::startDrag(Qt::DropActions supportedActions) {
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
    qDebug() << "startDrag() isRightClick:" << isRightClick;
    // 4. Drag ausführen und explizit alle drei Aktionen erlauben
    if (isRightClick) {
        drag->exec(supportedActions | Qt::CopyAction | Qt::MoveAction | Qt::LinkAction);
    } else {
        drag->exec(supportedActions | Qt::CopyAction | Qt::MoveAction);
    }
}

void CustomListView::dragMoveEvent(QDragMoveEvent *event) {
    QModelIndex index = indexAt(event->position().toPoint());
    if (!index.isValid()) { // empty background area
        // Wenn die Quelle aus uns selbst stammt (interner Drag), verbieten wir es SOFORT
        if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") && event->dropAction() == Qt::MoveAction) {
            event->ignore();
            return;
        }
    }

    QListView::dragMoveEvent(event);

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

void CustomListView::dropEvent(QDropEvent *event) {
    bool isRightClick = event->mimeData()->hasFormat("application/x-rightclickdrag");
    qDebug() << "dropEvent() isRightClick:" << isRightClick;
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

    QListView::dropEvent(event);
}

bool CustomListView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) {
    // Part of mitigation against going into rename mode from clicking a selected item while more than one is selected
    if (trigger == QAbstractItemView::SelectedClicked && m_hadMultiSelectionBeforeClick) {
        return false;
    }

    return QListView::edit(index, trigger, event);
}

void CustomListView::mousePressEvent(QMouseEvent *event) {
    // Part of mitigation against going into rename mode from clicking a selected item while more than one is selected
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

    // Part of mitigation for Shift+Up/Shift+Down range selection bug
    QModelIndex clicked = indexAt(event->pos());
    if (clicked.isValid() && !(event->modifiers() & Qt::ShiftModifier)) {
        m_selectionAnchor = clicked;
    }

    QListView::mousePressEvent(event);
}

// Part of mitigation for Shift+Up/Shift+Down range selection bug
void CustomListView::keyPressEvent(QKeyEvent *event) {
    bool shiftPressed = event->modifiers() & Qt::ShiftModifier;

    // Qt die normale Bewegung machen lassen (ändert den currentIndex)
    QListView::keyPressEvent(event);

    // Wenn KEIN Shift gedrückt war, wandert der Selektions-Anker stur mit
    if (!shiftPressed) {
        m_selectionAnchor = currentIndex();
    }
}

// Part of mitigation for Shift+Up/Shift+Down range selection bug
void CustomListView::setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) {
    bool isShift = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
    bool isDragging = QGuiApplication::mouseButtons() & Qt::LeftButton;
    QModelIndex current = currentIndex();

    if (!isShift || isDragging || !current.isValid() || !m_selectionAnchor.isValid()) {
        QListView::setSelection(rect, command);
        return;
    }

    int startRow = qMin(m_selectionAnchor.row(), current.row());
    int endRow = qMax(m_selectionAnchor.row(), current.row());

    QItemSelection linearSelection;
    for (int r = startRow; r <= endRow; ++r) {
        QModelIndex idx = model()->index(r, 0, rootIndex());
        if (idx.isValid()) {
            linearSelection.append(QItemSelectionRange(idx));
        }
    }

    // Wir füttern das Selektionsmodell direkt mit unserer mathematisch exakten Range
    if (selectionModel()) {
        selectionModel()->select(linearSelection, command);
    }
}
