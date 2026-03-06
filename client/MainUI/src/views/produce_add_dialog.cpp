#include "produce_add_dialog.h"
#include "ui_produce_add_dialog.h"

ProduceAddDialog::ProduceAddDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProduceAddDialog)
{
    ui->setupUi(this);
}

ProduceAddDialog::~ProduceAddDialog()
{
    delete ui;
}
