#include "FormTools.h"
#include "FormCheckTools.h"
#include "FormHex2ASCII.h"
#include "FormNavigation.h"
#include "FormTimeConvert.h"
#include "FormStopwatch.h"
#include "FormHexSplit.h"
#include "ui_FormTools.h"
FormTools::FormTools(QWidget *parent) : QWidget(parent), ui(new Ui::FormTools) {
    ui->setupUi(this);
    const QList<QPushButton *> buttons = {ui->button_checkTools, ui->button_convert,
                                          ui->button_navigation, ui->button_time,
                                          ui->button_stopwatch,  ui->button_split};
    for (int i = 0; i < buttons.size(); ++i)
        connect(buttons[i], &QPushButton::clicked, this, [this, i] { openTool(i); });
}
FormTools::~FormTools() {
    delete ui;
}
void FormTools::openTool(int index) {
    if (!m_windows.value(index)) {
        QWidget *window = nullptr;
        switch (index) {
        case 0:
            window = new FormCheckTools(this);
            break;
        case 1: {
            auto *convert = new FormHex2ASCII(this);
            connect(convert, &FormHex2ASCII::showLog, this, &FormTools::showLog);
            window = convert;
            break;
        }
        case 2:
            window = new FormNavigation(this);
            break;
        case 3:
            window = new FormTimeConvert(this);
            window->setWindowTitle("时间实时转换");
            break;
        case 4:
            window = new FormStopwatch(this);
            break;
        case 5:
            window = new FormHexSplit(this);
            break;
        default:
            return;
        }
        window->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint |
                               Qt::WindowMaximizeButtonHint);
        m_windows[index] = window;
    }
    auto *window = m_windows[index].data();
    window->show();
    window->raise();
    window->activateWindow();
}
