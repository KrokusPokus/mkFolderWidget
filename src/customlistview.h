#ifndef CUSTOMLISTVIEW_H
#define CUSTOMLISTVIEW_H

#include <QListView>
#include <QDragMoveEvent>
#include <QDropEvent>

class CustomListView : public QListView {
    Q_OBJECT

public:
    explicit CustomListView(QWidget *parent = nullptr);

private:
    bool m_hadMultiSelectionBeforeClick = false;
    QModelIndex m_selectionAnchor;


protected:
    // Hier deklarieren wir die Methoden, die wir überschreiben wollen
    void startDrag(Qt::DropActions supportedActions) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) override;
};

#endif // CUSTOMLISTVIEW_H
