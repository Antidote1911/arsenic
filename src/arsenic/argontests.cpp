#include "argontests.h"
#include "ui_argontests.h"

Argontests::Argontests(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Argontests)
{
    ui->setupUi(this);
}

Argontests::~Argontests()
{
    delete ui;
}
