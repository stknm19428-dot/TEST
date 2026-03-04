#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QLineSeries>
#include <QChart>
#include <QValueAxis>
#include <QChartView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QCryptographicHash>
#include <QDebug>

// for debug
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::connectDB()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");

    db.setHostName("localhost");
    db.setDatabaseName("Smart_MES_Core");
    db.setUserName("admin");
    db.setPassword("pw1234");

    qDebug() << QSqlDatabase::drivers();
    if (!db.open()) {
        qDebug() << "DB Connection Error:" << db.lastError().text();
        return false;
    }

    qDebug() << "DB Connected";
    return true;
}

QString sha512_hash(const QString &password, const QString &salt)
{
    QByteArray salted = (password + salt).toUtf8();
    QByteArray hash = QCryptographicHash::hash(salted, QCryptographicHash::Sha512);
    return hash.toHex();
}

void MainWindow::on_loginBtn_clicked()
{
    QString user = ui->userEdit->text();
    QString pw   = ui->passEdit->text();

    if (!connectDB())
        return;

    QSqlQuery query;
    query.prepare(
        "SELECT p.password_hash, p.salt "
        "FROM user u "
        "JOIN user_password p ON u.id = p.user_id "
        "WHERE u.user_name = :username"
        );

    query.bindValue(":username", user);

    if (!query.exec()) {
        qDebug() << "Query Error:" << query.lastError().text();
        return;
    }

    if (!query.next()) {
        qDebug() << "User not found";
        return;
    }

    QString db_hash = query.value(0).toString();
    QString db_salt = query.value(1).toString();

    QString challenge_hash = sha512_hash(pw, db_salt);

    if (db_hash == challenge_hash)
    {
        qDebug() << "Login SUCCESS";
        ui->stackedWidget->setCurrentIndex(1);
        showChart();
    }
    else
    {
        qDebug() << "Password mismatch";
    }
}

void MainWindow::showChart()
{
    QLineSeries *series = new QLineSeries();

    // 테스트용 더미 데이터
    series->append(0, 10);
    series->append(1, 25);
    series->append(2, 15);
    series->append(3, 30);
    series->append(4, 22);
    series->append(5, 40);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Test Line Chart");

    QValueAxis *axisX = new QValueAxis;
    axisX->setRange(0, 5);

    QValueAxis *axisY = new QValueAxis;
    axisY->setRange(0, 50);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    series->attachAxis(axisX);
    series->attachAxis(axisY);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    ui->chartLayout->addWidget(chartView);
}

