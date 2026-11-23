#ifndef FORMFILERANGE_H
#define FORMFILERANGE_H

#include "AppFileTools.h"
#include <QWidget>
namespace Ui {
class FormFileRange;
}

class FormFileRange : public QWidget {
    Q_OBJECT
public:
    explicit FormFileRange(QWidget *parent = nullptr);
    ~FormFileRange();
    QString sourcePath() const;
    bool selectedRange(AppFileTools::Range &range, QString &error) const;
signals:
    void rangeChanged();

private:
    void updateRange();
    void changeBase(bool hex);
    bool inputRange(AppFileTools::Range &range, QString &error) const;
    bool validateFileRange(const AppFileTools::Range &range, QString &error) const;
    bool m_hex = false;
    bool m_lengthDriven = false;
    Ui::FormFileRange *ui;
};
#endif
