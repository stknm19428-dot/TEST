#ifndef PROCESS_WIDGET_H
#define PROCESS_WIDGET_H
#include "base/base_page_widget.h"
#include <QWidget>
#include <QTreeWidgetItem>

namespace Ui {
class ProcessWidget;
}

class ProcessWidget : public BasePageWidget // 0308 haesung changed it 'QWidget' into 'BasePageWidget'
{
    Q_OBJECT

public:
    explicit ProcessWidget(QWidget *parent = nullptr);
    ~ProcessWidget();

private slots:
    void on_stop_clicked(const QString &process_name);
    void on_start_clicked(const QString &process_name);
    void on_Back_btn_clicked();

private:
    Ui::ProcessWidget *ui;

    void setup_process_tree();
    void add_process_item(QTreeWidgetItem *parent_item,
                          const QString &process_name);
};

#endif // PROCESS_WIDGET_H
