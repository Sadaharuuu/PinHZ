#ifndef FORMNAVIGATION_H
#define FORMNAVIGATION_H
#include <QDialog>
#include "AppNavigation.h"
namespace Ui {
class FormNavigation;
}
class FormNavigation : public QDialog {
    Q_OBJECT
public:
    explicit FormNavigation(QWidget *parent = nullptr);
    ~FormNavigation();

private:
    Ui::FormNavigation *ui;
    bool m_updatingCoordinates = false;
    bool m_hasCoordinates = false;
    bool m_updatingAngle = false;
    int m_sourceColumn = 0;
    navigation::Vec3 m_xyz{}, m_blh{};
    void coordinateEdited(int column);
    void fillCoordinates(int sourceColumn);
    bool readOrigin(navigation::Vec3 &origin, double rotation[9]) const;
    void originEdited();
    void angleFormatChanged();
    void angleEdited(bool radians);
};
#endif
