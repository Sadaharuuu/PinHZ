#ifndef FORMTOOLS_H
#define FORMTOOLS_H
#include <QWidget>
#include <QHash>
#include <QPointer>
#include "CommonDefine.h"
namespace Ui {
class FormTools;
}
class FormTools : public QWidget {
    Q_OBJECT
public:
    explicit FormTools(QWidget *parent = nullptr);
    ~FormTools();

signals:
    void showLog(LogLevel level, QString message);

private:
    QHash<int, QPointer<QWidget>> m_windows;
    void openTool(int index);
    Ui::FormTools *ui;
};
#endif
