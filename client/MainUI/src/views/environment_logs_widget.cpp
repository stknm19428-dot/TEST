#include "environment_logs_widget.h"
#include "ui_environment_logs_widget.h"


#include <QDebug>
#include <QFont>
#include <QTextOption>
#include <QPushButton>
#include <QComboBox>

// for clean error logs    this function keep the width of string with space
QString EnvironmentLogsWidget::pad(const QString &s, int w)
{
    if (s.size() >= w) return s.left(w);
    return s + QString(w - s.size(), ' ');
}



// Constructor
EnvironmentLogsWidget::EnvironmentLogsWidget(QWidget *parent)
    : BasePageWidget(parent),
    ui(new Ui::EnvironmentLogsWidget)
{
    ui->setupUi(this);

    QFont mono("Monospace"); // keep string aligned
    mono.setStyleHint(QFont::TypeWriter);
    ui->logView->setFont(mono);

    ui->logView->setWordWrapMode(QTextOption::NoWrap);
    ui->logView->setReadOnly(true); // disable users to adjus logs



    connect(ui->clearButton, &QPushButton::clicked,
            this, &EnvironmentLogsWidget::onClearClicked);

    // line/type combo box changed -> redraw filtered logs
    connect(ui->lineCombo, &QComboBox::currentTextChanged,
            this, &EnvironmentLogsWidget::refreshLogView);

    connect(ui->typeCombo, &QComboBox::currentTextChanged,
            this, &EnvironmentLogsWidget::refreshLogView);

    //data load
    loadAllData();

    // call loadAllData per 2 seconds

    timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, &EnvironmentLogsWidget::loadAllData);
    timer->start();
}

EnvironmentLogsWidget::~EnvironmentLogsWidget()
{
    delete ui;
}



// check whether one log passes current filter
bool EnvironmentLogsWidget::passesFilter(const LogEntry &log) const
{
    const QString selectedLine = ui->lineCombo->currentText();
    const QString selectedType = ui->typeCombo->currentText();

    const bool lineMatch = (selectedLine == "ALL" || log.line == selectedLine);
    const bool typeMatch = (selectedType == "ALL" || log.type == selectedType);

    return lineMatch && typeMatch;
}


// redraw logView using allLogs + current filter
void EnvironmentLogsWidget::refreshLogView()
{
    ui->logView->clear();

    for (const LogEntry &log : allLogs) {
        if (!passesFilter(log))
            continue;

        QString row =
            pad(log.time, W_TIME) + "  |" +
            pad(log.line, W_LINE) + "    |" +
            pad(log.type, W_TYPE) + "   |" +
            log.message;

        ui->logView->appendPlainText(row);
    }
}


void EnvironmentLogsWidget::loadAllData()
{
    EnvironmentLogsService service;
    QVector<LogEntry> newLogs;

    if (firstLoad) {
        newLogs = service.fetchRecentLogs();
    } else {
        newLogs = service.fetchLogsAfter(lastEventAt);
    }

    if (newLogs.isEmpty()) {
        if (firstLoad)
            firstLoad = false;
        return;
    }

    QDateTime newestEventAt = lastEventAt;

    if (firstLoad) {
        for (int i = newLogs.size() - 1; i >= 0; --i) {
            allLogs.append(newLogs[i]);
        }
        firstLoad = false;
    } else {
        for (const LogEntry &log : newLogs) {
            allLogs.append(log);
        }
    }

    for (const LogEntry &log : newLogs) {
        if (log.eventAt.isValid() && log.eventAt > newestEventAt)
            newestEventAt = log.eventAt;
    }

    if (newestEventAt.isValid())
        lastEventAt = newestEventAt;

    refreshLogView();
}







// clear filter, not original logs
void EnvironmentLogsWidget::onClearClicked()
{
    allLogs.clear();
    ui->logView->clear();

    firstLoad = false;
    lastEventAt = QDateTime::currentDateTime();
}


void EnvironmentLogsWidget::on_Back_btn_clicked()
{
    emit requestPageChange(PageType::Dashboard);
}

