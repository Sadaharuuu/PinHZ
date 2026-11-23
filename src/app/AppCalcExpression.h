#ifndef APPCALCEXPRESSION_H
#define APPCALCEXPRESSION_H

#include <QString>

// A bounded arithmetic parser. Expressions cannot execute code or access application state.
class AppCalcExpression {
public:
    static bool validate(const QString &expression, QString &error);
    static bool evaluate(const QString &expression, double x, double &result, QString &error);
};

#endif
