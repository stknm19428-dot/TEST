#include "environment_logs_widget.h"
#include "ui_environment_logs_widget.h"

#include "../core/database_manager.h"

#include <QSqlQuery>
#include <QSqlError>
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




// connect to database and implement query
// save logs in allLogs, then draw filtered result on logView
void EnvironmentLogsWidget::loadAllData()
{
    if (!DatabaseManager::instance().connect()) {
        qDebug() << "[ENV] Database connect failed!";
        return;
    }

    QSqlQuery query;

    if (firstLoad) {
        // first load: bring recent 200 logs
        query.prepare(
            "SELECT process_id, sensor_type, sensor_value, description, event_at "
            "FROM environment_logs "
            "ORDER BY event_at DESC "
            "LIMIT 200"
            );
    } else {
        // after first load: only bring new logs
        query.prepare(
            "SELECT process_id, sensor_type, sensor_value, description, event_at "
            "FROM environment_logs "
            "WHERE event_at > :lastEventAt "
            "ORDER BY event_at ASC"
            );
        query.bindValue(":lastEventAt", lastEventAt);
    }

    if (!query.exec()) {
        qDebug() << "[ENV] SELECT failed:" << query.lastError().text();
        return;
    }

    QVector<LogEntry> newLogs;
    QDateTime newestEventAt = lastEventAt;

    while (query.next()) {
        const QString processId = query.value("process_id").toString();
        const QString value     = query.value("sensor_value").toString();
        const QString desc      = query.value("description").toString();
        const QDateTime eventAt = query.value("event_at").toDateTime();
        const QString t         = eventAt.toString("yyyy-MM-dd HH:mm:ss");

        QString line;
        if (processId.startsWith("63014862-922"))
            line = "Manufacture";
        else if (processId.startsWith("1faeda99-118"))
            line = "Logistics";
        else if (processId.startsWith("9f87460c-14f"))
            line = "ALL";
        else
            line = "-";

        QString typeLabel;
        if (desc.contains("FIRE", Qt::CaseInsensitive) ||
            desc.contains("STOPPED", Qt::CaseInsensitive))
        {
            typeLabel = "WARNING";
        }
        else if (desc.contains("ABNORMAL", Qt::CaseInsensitive) ||
                 desc.contains("OUT OF STOCK", Qt::CaseInsensitive) ||
                 desc.contains("STANDARD", Qt::CaseInsensitive))
        {
            typeLabel = "INFO";
        }
        else
        {
            typeLabel = "UNKNOWN";
        }

        LogEntry log;
        log.time = t.left(W_TIME);
        log.line = line;
        log.type = typeLabel;
        log.message = desc + (value.isEmpty() ? "" : (" (" + value + ")"));

        newLogs.append(log);

        if (eventAt.isValid() && eventAt > newestEventAt)
            newestEventAt = eventAt;
    }

    if (newLogs.isEmpty()) {
        if (firstLoad)
            firstLoad = false;
        return;
    }

    if (firstLoad) {
        // first query was DESC, so reverse before storing
        // to show old -> new from top to bottom
        for (int i = newLogs.size() - 1; i >= 0; --i) {
            allLogs.append(newLogs[i]);
        }
        firstLoad = false;
    } else {
        // new logs already came in ASC order
        for (const LogEntry &log : newLogs) {
            allLogs.append(log);
        }
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

