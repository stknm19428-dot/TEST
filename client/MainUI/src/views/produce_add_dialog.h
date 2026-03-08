#ifndef PRODUCE_ADD_DIALOG_H
#define PRODUCE_ADD_DIALOG_H

#include <QDialog>

namespace Ui {
class ProduceAddDialog;
}

class ProduceAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProduceAddDialog(QWidget *parent = nullptr);
    ~ProduceAddDialog();

private:
    Ui::ProduceAddDialog *ui;
};

#endif // PRODUCE_ADD_DIALOG_H
