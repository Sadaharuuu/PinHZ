#ifndef FORMFILLITEM_H
#define FORMFILLITEM_H

#include <QWidget>
#include <QByteArray>

namespace Ui {
class FormFillItem;
}

class FormFillItem : public QWidget {
    Q_OBJECT

public:
    explicit FormFillItem(QWidget *parent = 0);
    ~FormFillItem();

    enum class FillMode : int8_t { Order = 0, Repeat };

    bool m_isFillValid = false;
    QByteArray m_fillData;
    int32_t m_fillStart = 0;
    int32_t m_fillRowCnt = 0;
    int32_t m_fillBytes = 0;
    FillMode m_fillMode = FillMode::Order;

signals:
    fillConfDone(bool isValid);
private slots:
    void on_button_fill_clicked();
    void on_groupCheck_clicked();

private:
    Ui::FormFillItem *ui;
};

#endif // FORMFILLITEM_H
