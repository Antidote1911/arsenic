#include "config.h"
#include "ui_config.h"
#include "constants.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>

Config::Config(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Config)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &Config::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &Config::reject);
}

Config::~Config()
{
    delete ui;
}

bool Config::getPreferences(ARs::Preferences &prefs, QWidget *parent)
{
    bool accepted = false;

    Config dialog(parent);
    dialog.ui->editExtension->setText(prefs.extension);
    dialog.ui->comboMemory->setCurrentIndex(prefs.argonMemory);
    dialog.ui->comboOps->setCurrentIndex(prefs.argonItr);
    dialog.ui->comboAlgo->setCurrentText(prefs.cryptoAlgo);
    dialog.ui->editName->setText(prefs.userName);

    if (dialog.exec() == QDialog::Accepted)
    {
        prefs.extension    = dialog.ui->editExtension->text();
        prefs.argonMemory  = dialog.ui->comboMemory->currentIndex();
        prefs.argonItr     = dialog.ui->comboOps->currentIndex();
        prefs.cryptoAlgo     = dialog.ui->comboAlgo->currentText();
        prefs.userName    = dialog.ui->editName->text();

        accepted = true;
    }


    return accepted;
}
