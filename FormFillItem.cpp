#include "FormFillItem.h"
#include "ui_FormFillItem.h"

FormFillItem::FormFillItem(QWidget *parent) : QWidget(parent),
                                              ui(new Ui::FormFillItem)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    connect(ui->groupBox_step, &QGroupBox::clicked, this, on_groupCheck_clicked);
    connect(ui->groupBox_repeat, &QGroupBox::clicked, this, on_groupCheck_clicked);
}

FormFillItem::~FormFillItem()
{
    delete ui;
}

extern int32_t g_fillBuf[1024];
extern int32_t g_fillRowCnt;
extern int32_t g_fillBytes;

void FormFillItem::on_button_fill_clicked()
{
    //    emit showLog(LogLevel_WAR, "取消设置");
    int32_t fillData = 0;
    int32_t valueBase = 0;
    // 判断类型
    if (ui->groupBox_step->isChecked())
    {
        // 顺序填充
        m_fillStatus = 1;
        int32_t step = ui->spin_step->value();
        if (step <= 0)
        {
            m_isFillValid = true;
            emit fillConfDone();
            return;
        }
        valueBase = ui->check_startValue->isChecked() ? 16 : 10;
        int32_t startValue = ui->lineEdit_startValue->text().toInt(&m_isFillValid, valueBase);
        if (!m_isFillValid)
        {
            emit fillConfDone();
            return;
        }
        valueBase = ui->check_endValue->isChecked() ? 16 : 10;
        int32_t endValue = ui->lineEdit_endValue->text().toInt(&m_isFillValid, valueBase);
        if (!m_isFillValid)
        {
            emit fillConfDone();
            return;
        }
        fillData = startValue;
        for (int32_t i = 0; i < g_fillRowCnt; i++)
        {
            g_fillBuf[i] = fillData;
            if (startValue <= endValue && fillData <= endValue - step)
                fillData += step; // 正序填充
            else if (startValue > endValue && fillData >= endValue + step)
                fillData -= step; // 逆序填充
            else
            {
                memset(g_fillBuf + i + 1, 0, (g_fillRowCnt - i) * sizeof(int32_t));
                break;
            }
        }
    }
    else
    {
        // 重复填充
        m_fillStatus = 2;
        QString str = ui->lineEdit_repeat->text();
        valueBase = 16;
        uint8_t repeatBuf[1024] = {0,};
        int32_t fillIndex = 0;
        if (str.length() % 2 != 0)
        {
            str = "0" + str;
        }
        for (int32_t i = 0; i < str.length(); i += 2)
        {
            repeatBuf[fillIndex++] = str.mid(i, 2).toUInt(&m_isFillValid, valueBase);
            if (!m_isFillValid)
            {
                emit fillConfDone();
                return;
            }
        }

        for (int32_t i = 0; ; i++)
        {
            if (fillIndex * (i + 1) < g_fillBytes)
                memcpy((uint8_t *)g_fillBuf + fillIndex * i, repeatBuf, fillIndex);
            else
            {
                memcpy((uint8_t *)g_fillBuf + fillIndex * i, repeatBuf, g_fillBytes - fillIndex * i);
                break;
            }
        }
    }
    close();
    emit fillConfDone();
}

void FormFillItem::on_groupCheck_clicked()
{
    // 填充类型二选一
    if (ui->groupBox_step == qobject_cast<QGroupBox *>(sender()))
    {
        // 顺序填充
        ui->groupBox_repeat->setChecked(false);
    }
    else
    {
        // 重复填充
        ui->groupBox_step->setChecked(false);
    }
}
