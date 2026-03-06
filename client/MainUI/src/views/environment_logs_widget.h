#ifndef ENVIRONMENT_LOGS_WIDGET_H
#define ENVIRONMENT_LOGS_WIDGET_H

#include "base/base_page_widget.h"

#include <QWidget>
#include <QTimer>
#include <QString>
#include <QVector>
#include <QDateTime>

QT_BEGIN_NAMESPACE
namespace Ui { class EnvironmentLogsWidget; }
QT_END_NAMESPACE

struct LogEntry
{
    QString time;
    QString line;
    QString type;
    QString message;
};

class EnvironmentLogsWidget : public BasePageWidget
{
    Q_OBJECT

public:
    explicit EnvironmentLogsWidget(QWidget *parent = nullptr);
    ~EnvironmentLogsWidget();

public slots:
    void loadAllData();

private slots:
    void onClearClicked();
    void refreshLogView();
    void on_Back_btn_clicked();


private:
    QString pad(const QString &s, int w);
    bool passesFilter(const LogEntry &log) const;

    QVector<LogEntry> allLogs;

    Ui::EnvironmentLogsWidget *ui = nullptr;
    QTimer *timer = nullptr;

    QDateTime lastEventAt;
    bool firstLoad = true;

    static constexpr int W_TIME = 23;
    static constexpr int W_LINE = 12;
    static constexpr int W_TYPE = 10;
};

#endif // ENVIRONMENT_LOGS_WIDGET_H
