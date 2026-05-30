#ifndef CUSTOMTABLEVIEW_H
#define CUSTOMTABLEVIEW_H

#include <QTableView>
#include <QDragMoveEvent>
#include <QDropEvent>

class CustomTableView : public QTableView {
    Q_OBJECT

public:
    // Der Konstruktor reicht einfach den Parent an die Basisklasse weiter
    explicit CustomTableView(QWidget *parent = nullptr);

private:
    bool m_hadMultiSelectionBeforeClick = false;

protected:
    // Hier deklarieren wir die Methoden, die wir überschreiben wollen
    void startDrag(Qt::DropActions supportedActions) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
};

#endif // CUSTOMTABLEVIEW_H
