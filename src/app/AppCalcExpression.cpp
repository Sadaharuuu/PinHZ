#include "AppCalcExpression.h"
#include <QVector>
#include <QStringList>
#include <cmath>
#include <algorithm>

namespace {
class Parser {
public:
    Parser(QString expression, double x, bool calculate)
        : source(expression.trimmed()), variable(x), calculating(calculate) {}
    bool run(double &result, QString &error) {
        if (source.isEmpty())
            source = "x";
        if (source.size() > 2048)
            fail("公式不能超过 2048 个字符");
        result = message.isEmpty() ? sum() : 0;
        space();
        if (pos != source.size())
            fail("多余字符或缺少运算符（乘法请写 *）");
        error = message;
        return error.isEmpty();
    }

private:
    QString source, message;
    int pos = 0, depth = 0;
    double variable;
    bool calculating;
    void fail(const QString &reason) {
        if (message.isEmpty())
            message = QString("第 %1 个字符：%2").arg(pos + 1).arg(reason);
    }
    void space() {
        while (pos < source.size() && source.at(pos).isSpace())
            ++pos;
    }
    bool take(QChar c) {
        space();
        if (pos < source.size() && source.at(pos) == c) {
            ++pos;
            return true;
        }
        return false;
    }
    double finite(double value) {
        if (calculating && !std::isfinite(value))
            fail("结果无效，请检查函数定义域或数值溢出");
        return value;
    }
    double sum() {
        double value = product();
        while (message.isEmpty()) {
            if (take('+'))
                value = finite(value + product());
            else if (take('-'))
                value = finite(value - product());
            else
                break;
        }
        return value;
    }
    double product() {
        double value = unary();
        while (message.isEmpty()) {
            if (take('*'))
                value = finite(value * unary());
            else if (take('/')) {
                double right = unary();
                if (calculating && right == 0)
                    fail("不能除以零");
                if (calculating && right != 0)
                    value = finite(value / right);
            } else if (take('%')) {
                double right = unary();
                if (calculating && right == 0)
                    fail("取余的除数不能为零");
                if (calculating && right != 0)
                    value = finite(std::fmod(value, right));
            } else
                break;
        }
        return value;
    }
    double unary() {
        if (++depth > 64) {
            fail("公式嵌套过深");
            --depth;
            return 0;
        }
        double value;
        if (take('+'))
            value = unary();
        else if (take('-'))
            value = -unary();
        else
            value = power();
        --depth;
        return value;
    }
    double power() {
        double value = atom();
        if (message.isEmpty() && take('^')) {
            const double right = unary(); // right associative; -2^2 == -(2^2)
            if (calculating)
                value = finite(std::pow(value, right));
        }
        return value;
    }
    double function(const QString &name, const QVector<double> &args) {
        const QStringList one{"log",   "ln",   "log10", "log2",  "exp",  "sqrt", "abs",  "sin",
                              "cos",   "tan",  "asin",  "acos",  "atan", "sinh", "cosh", "tanh",
                              "floor", "ceil", "round", "trunc", "deg",  "rad"};
        const QStringList two{"pow", "min", "max", "atan2"};
        const int count = one.contains(name) ? 1 : two.contains(name) ? 2 : name == "clamp" ? 3 : 0;
        if (!count) {
            fail("未知函数：" + name);
            return 0;
        }
        if (args.size() != count) {
            fail(QString("%1 需要 %2 个参数").arg(name).arg(count));
            return 0;
        }
        if (!calculating)
            return 0;
        const double a = args[0], b = count >= 2 ? args[1] : 0;
        double result = 0;
        if (name == "log" || name == "ln")
            result = std::log(a);
        else if (name == "log10")
            result = std::log10(a);
        else if (name == "log2")
            result = std::log2(a);
        else if (name == "exp")
            result = std::exp(a);
        else if (name == "sqrt")
            result = std::sqrt(a);
        else if (name == "abs")
            result = std::fabs(a);
        else if (name == "sin")
            result = std::sin(a);
        else if (name == "cos")
            result = std::cos(a);
        else if (name == "tan")
            result = std::tan(a);
        else if (name == "asin")
            result = std::asin(a);
        else if (name == "acos")
            result = std::acos(a);
        else if (name == "atan")
            result = std::atan(a);
        else if (name == "sinh")
            result = std::sinh(a);
        else if (name == "cosh")
            result = std::cosh(a);
        else if (name == "tanh")
            result = std::tanh(a);
        else if (name == "floor")
            result = std::floor(a);
        else if (name == "ceil")
            result = std::ceil(a);
        else if (name == "round")
            result = std::round(a);
        else if (name == "trunc")
            result = std::trunc(a);
        else if (name == "deg")
            result = a * 180 / std::acos(-1.0);
        else if (name == "rad")
            result = a * std::acos(-1.0) / 180;
        else if (name == "pow")
            result = std::pow(a, b);
        else if (name == "min")
            result = std::min(a, b);
        else if (name == "max")
            result = std::max(a, b);
        else if (name == "atan2")
            result = std::atan2(a, b);
        else if (name == "clamp") {
            if (b > args[2])
                fail("clamp 下限不能大于上限");
            result = std::max(b, std::min(a, args[2]));
        }
        return finite(result);
    }
    double atom() {
        space();
        if (!message.isEmpty())
            return 0;
        if (take('(')) {
            double value = sum();
            if (!take(')'))
                fail("缺少右括号");
            return value;
        }
        if (pos >= source.size()) {
            fail("缺少数值或变量 x");
            return 0;
        }
        const int start = pos;
        if (source.at(pos).isLetter()) {
            while (pos < source.size() && source.at(pos).isLetterOrNumber())
                ++pos;
            QString name = source.mid(start, pos - start).toLower();
            if (take('(')) {
                QVector<double> args;
                if (!take(')')) {
                    do {
                        args.append(sum());
                        if (args.size() > 3) {
                            fail("函数参数过多");
                            break;
                        }
                    } while (message.isEmpty() && take(','));
                    if (!take(')'))
                        fail("缺少右括号");
                }
                return function(name, args);
            }
            if (name == "x")
                return variable;
            if (name == "pi")
                return std::acos(-1.0);
            if (name == "e")
                return std::exp(1.0);
            fail("未知变量：" + name);
            return 0;
        }
        bool digits = false;
        while (pos < source.size() && source.at(pos).isDigit()) {
            ++pos;
            digits = true;
        }
        if (take('.')) {
            while (pos < source.size() && source.at(pos).isDigit()) {
                ++pos;
                digits = true;
            }
        }
        if (!digits) {
            fail("需要数值、函数或变量 x");
            return 0;
        }
        if (pos < source.size() && source.at(pos).toLower() == 'e') {
            ++pos;
            if (pos < source.size() && (source.at(pos) == '+' || source.at(pos) == '-'))
                ++pos;
            const int exponent = pos;
            while (pos < source.size() && source.at(pos).isDigit())
                ++pos;
            if (exponent == pos)
                fail("科学计数法缺少指数");
        }
        bool ok = false;
        const double value = source.mid(start, pos - start).trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value))
            fail("数值格式错误或数值溢出");
        return value;
    }
};
} // namespace

bool AppCalcExpression::validate(const QString &expression, QString &error) {
    double unused = 0;
    return Parser(expression, 0, false).run(unused, error);
}

bool AppCalcExpression::evaluate(const QString &expression, double x, double &result,
                                 QString &error) {
    if (!std::isfinite(x)) {
        error = "输入值必须是有限数值";
        return false;
    }
    return Parser(expression, x, true).run(result, error);
}
