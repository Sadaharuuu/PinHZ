#ifndef FORMHEX2ASCII_H
#define FORMHEX2ASCII_H

#include "CommonDefine.h"
#include <QWidget>

namespace Ui {
class FormHex2ASCII;
}

class FormHex2ASCII : public QWidget {
    Q_OBJECT

public:
    explicit FormHex2ASCII(QWidget *parent = 0);
    ~FormHex2ASCII();

signals:
    void hex2ASCIIDone(int8_t dealCode);
    void showLog(LogLevel level, QString string);
private slots:
    void on_button_confirm_clicked();

private:
    Ui::FormHex2ASCII *ui;
    void updateText(bool fromHex);

    enum class ConvertMode : int8_t { hex2ASCII = 0, ASCII2Hex };
    bool convertHex2ASCII(QString srcPath, QString dstPath);
    bool convertASCII2Hex(QString srcPath, QString dstPath);
};

#endif // FORMHEX2ASCII_H
