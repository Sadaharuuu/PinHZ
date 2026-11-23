#include "FormCheckTools.h"
#include "ui_FormCheckTools.h"
#include <QPushButton>
FormCheckTools::FormCheckTools(QWidget *parent) : QDialog(parent), ui(new Ui::FormCheckTools) {
    ui->setupUi(this);
    for (QWidget *page : QList<QWidget *>{ui->dataCheck, ui->fileCheck}) {
        page->setWindowFlags(Qt::Widget);
        page->show();
    }
    for (auto *button : findChildren<QPushButton *>())
        if (button->objectName() == "button_close")
            button->hide();
}
FormCheckTools::~FormCheckTools() {
    delete ui;
}
