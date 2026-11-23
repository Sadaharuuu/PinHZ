#ifndef FORMDATACHECK_H
#define FORMDATACHECK_H

#include <QDialog>
namespace Ui {
class FormDataCheck;
}

class FormDataCheck : public QDialog {
    Q_OBJECT
public:
    explicit FormDataCheck(QWidget *parent = nullptr);
    ~FormDataCheck();

private:
    void calculateInput();
    Ui::FormDataCheck *ui;
};
#endif
