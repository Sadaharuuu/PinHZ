#ifndef FORMCHECKPANEL_H
#define FORMCHECKPANEL_H

#include "AppCalcCheck.h"
#include <QWidget>

namespace Ui {
class FormCheckPanel;
}
class FormCRCConf;

class FormCheckPanel : public QWidget {
    Q_OBJECT
public:
    explicit FormCheckPanel(QWidget *parent = nullptr);
    ~FormCheckPanel();
    AppCalcCheck::Settings settings() const;
    void clearResult();
    void setStatus(const QString &text);
    void showResult(const QByteArray &result, const QString &status);
signals:
    void settingsChanged();

private:
    void updateType();
    Ui::FormCheckPanel *ui;
    FormCRCConf *m_crcDialog;
    s_crc_conf m_crcConf;
};
#endif
