#ifndef FORMFILECHECK_H
#define FORMFILECHECK_H
#include <QDialog>
namespace Ui {
class FormFileCheck;
}

class FormFileCheck : public QDialog {
    Q_OBJECT
public:
    explicit FormFileCheck(QWidget *parent = nullptr);
    ~FormFileCheck();

private:
    void calculateFile();
    Ui::FormFileCheck *ui;
};
#endif
