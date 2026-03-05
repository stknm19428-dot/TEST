#ifndef PROCESS_WIDGET_H
#define PROCESS_WIDGET_H

#include <QWidget>
#include <QTreeWidgetItem>

namespace Ui {
class ProcessWidget;
}

class ProcessWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessWidget(QWidget *parent = nullptr);
    ~ProcessWidget();

private slots:
    void on_stop_clicked(const QString &process_name);

private:
    Ui::ProcessWidget *ui;

    void setup_process_tree();
    void add_process_item(QTreeWidgetItem *parent_item,
                          const QString &process_name);
};

#endif // PROCESS_WIDGET_H
