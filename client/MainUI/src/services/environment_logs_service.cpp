#include "environment_logs_service.h"
#include "../core/database_manager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

QString EnvironmentLogsService::resolveLine(const QString &processId) const
{
    if (processId.startsWith("63014862-922"))
        return "Manufacture";
    else if (processId.startsWith("1faeda99-118"))
        return "Logistics";
    else if (processId.startsWith("9f87460c-14f"))
        return "ALL";
    else
        return "-";
}

QString EnvironmentLogsService::resolveType(const QString &desc) const
{
    if (desc.contains("FIRE", Qt::CaseInsensitive) ||
        desc.contains("STOPPED", Qt::CaseInsensitive))
    {
        return "WARNING";
    }
    else if (desc.contains("ABNORMAL", Qt::CaseInsensitive) ||
             desc.contains("OUT OF STOCK", Qt::CaseInsensitive) ||
             desc.contains("STANDARD", Qt::CaseInsensitive))
    {
        return "INFO";
    }
    else
    {
        return "UNKNOWN";
    }
}

QVector<LogEntry> EnvironmentLogsService::fetchRecentLogs()
{
    QVector<LogEntry> logs;

    if (!DatabaseManager::instance().connect()) {
        qDebug() << "[ENV] Database connect failed!";
        return logs;
    }

    QSqlQuery query;
    query.prepare(
        "SELECT process_id, sensor_type, sensor_value, description, event_at "
        "FROM environment_logs "
        "ORDER BY event_at DESC "
        "LIMIT 200"
        );

    if (!query.exec()) {
        qDebug() << "[ENV] SELECT failed:" << query.lastError().text();
        return logs;
    }

    while (query.next()) {
        const QString processId = query.value("process_id").toString();
        const QString value     = query.value("sensor_value").toString();
        const QString desc      = query.value("description").toString();
        const QDateTime eventAt = query.value("event_at").toDateTime();

        LogEntry log;
        log.time = eventAt.toString("yyyy-MM-dd HH:mm:ss");
        log.line = resolveLine(processId);
        log.type = resolveType(desc);
        log.message = desc + (value.isEmpty() ? "" : (" (" + value + ")"));
        log.eventAt = eventAt;

        logs.append(log);
    }

    return logs;
}

QVector<LogEntry> EnvironmentLogsService::fetchLogsAfter(const QDateTime &lastEventAt)
{
    QVector<LogEntry> logs;

    if (!DatabaseManager::instance().connect()) {
        qDebug() << "[ENV] Database connect failed!";
        return logs;
    }

    QSqlQuery query;
    query.prepare(
        "SELECT process_id, sensor_type, sensor_value, description, event_at "
        "FROM environment_logs "
        "WHERE event_at > :lastEventAt "
        "ORDER BY event_at ASC"
        );
    query.bindValue(":lastEventAt", lastEventAt);

    if (!query.exec()) {
        qDebug() << "[ENV] SELECT failed:" << query.lastError().text();
        return logs;
    }

    while (query.next()) {
        const QString processId = query.value("process_id").toString();
        const QString value     = query.value("sensor_value").toString();
        const QString desc      = query.value("description").toString();
        const QDateTime eventAt = query.value("event_at").toDateTime();

        LogEntry log;
        log.time = eventAt.toString("yyyy-MM-dd HH:mm:ss");
        log.line = resolveLine(processId);
        log.type = resolveType(desc);
        log.message = desc + (value.isEmpty() ? "" : (" (" + value + ")"));
        log.eventAt = eventAt;

        logs.append(log);
    }

    return logs;
}
