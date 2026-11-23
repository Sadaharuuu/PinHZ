#ifndef FORMTIMECONVERT_H
#define FORMTIMECONVERT_H
#include <QWidget>
#include <QDateTime>
#include <QStringList>
namespace Ui {
class FormTimeConvert;
}
class FormTimeConvert : public QWidget {
    Q_OBJECT
public:
    explicit FormTimeConvert(QWidget *parent = nullptr);
    ~FormTimeConvert();

private:
    Ui::FormTimeConvert *ui;
    bool m_updatingTime = false;
    int m_timeSourceRow = 0;
    QStringList m_timeSourceValues;
    QStringList fields(int row) const;
    void timeEdited(int row);
    bool calculate(QDateTime &utc, QString &error) const;
    void render(const QDateTime &utc, const QString &error);
};
#endif
