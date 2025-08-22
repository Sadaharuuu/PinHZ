#ifndef FORMFILLITEM_H
#define FORMFILLITEM_H

#include <QWidget>

namespace Ui
{
class FormFillItem;
}

class FormFillItem : public QWidget
{
    Q_OBJECT

public:
    explicit FormFillItem(QWidget *parent = 0);
    ~FormFillItem();

    bool m_isFillValid = false;
signals:
    fillConfDone();
private slots:
    void on_button_fill_clicked();
    void on_groupCheck_clicked();

private:
    Ui::FormFillItem *ui;
};

#endif // FORMFILLITEM_H
