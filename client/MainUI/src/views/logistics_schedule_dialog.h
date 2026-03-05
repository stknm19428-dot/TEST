#ifndef LOGISTICS_SCHEDULE_DIALOG_H
#define LOGISTICS_SCHEDULE_DIALOG_H

#include <QDialog>

namespace Ui {
class LogisticsScheduleDialog;
}

class LogisticsScheduleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogisticsScheduleDialog(QWidget *parent = nullptr);
    ~LogisticsScheduleDialog();

private slots:
    void on_confirm_button_clicked();
    void on_cancel_button_clicked();

private:
    Ui::LogisticsScheduleDialog *ui;
};

#endif // LOGISTICS_SCHEDULE_DIALOG_H
