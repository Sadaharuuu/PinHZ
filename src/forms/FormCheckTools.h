#ifndef FORMCHECKTOOLS_H
#define FORMCHECKTOOLS_H
#include <QDialog>
namespace Ui {
class FormCheckTools;
}
class FormCheckTools : public QDialog {
    Q_OBJECT
public:
    explicit FormCheckTools(QWidget *parent = nullptr);
    ~FormCheckTools();

private:
    Ui::FormCheckTools *ui;
};
#endif
