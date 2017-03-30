#include "config.h"
#include "ui_config.h"
#include <iostream>
#include <QComboBox>
#include <QSettings>
#include <QLabel>
#include <QCheckBox>
#include <../preferences/Constants.h>


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
    bool retValue = false;

    Config dialog(parent);

    dialog.ui->comboAlgo->setCurrentText(prefs.algoCrypto);
    dialog.ui->comboEncoding->setCurrentText(prefs.encoding);
    dialog.ui->checkDeleteOriginals->setChecked(prefs.deleteOriginals);


    if (dialog.exec() == QDialog::Accepted)
    {
        prefs.algoCrypto       = dialog.ui->comboAlgo->currentText();
        prefs.encoding         = dialog.ui->comboEncoding->currentText();
        prefs.deleteOriginals  = dialog.ui->checkDeleteOriginals->isChecked();
        retValue = true;
    }

    return retValue;
}
