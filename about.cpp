#include "about.h"
#include "ui_about.h"
#include <QTextBrowser>

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);

}

About::~About()
{
    delete ui;
}

void About::setText(QString source, QWidget *parent)
{
    About dialog(parent);
    dialog.ui->textBrowser->setSource(source);
    dialog.exec();
}
