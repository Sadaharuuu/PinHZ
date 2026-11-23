#ifndef FORMHEXSPLIT_H
#define FORMHEXSPLIT_H
#include <QDialog>
namespace Ui {
class FormHexSplit;
}

class FormHexSplit : public QDialog {
    Q_OBJECT
public:
    explicit FormHexSplit(QWidget *parent = nullptr);
    ~FormHexSplit();

private:
    void split();
    Ui::FormHexSplit *ui;
};
#endif
