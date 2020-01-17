#include "configDialog.h"
#include "Config.h"
#include "ui_configDialog.h"
#include "constants.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>

ConfigDialog::ConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);
    connect(this, SIGNAL(accepted()), SLOT(saveSettings()));
    //connect(this, SIGNAL(rejected()), SLOT(reject()));
    loadSettings();
}

ConfigDialog::~ConfigDialog()
{
    delete ui;
}

void ConfigDialog::loadSettings()
{
    if (config()->hasAccessError()) {
            QString warn_text = QString(tr("Access error for config file %1").arg(config()->getFileName()));
            QMessageBox::warning(this, tr("Could not load configuration"), warn_text);
        }

    ui->editExtension->setText(config()->get("extension").toString());
    ui->comboMemory->setCurrentIndex(config()->get("CRYPTO/argonMemory").toInt());
    ui->comboOps->setCurrentIndex(config()->get("CRYPTO/argonItr").toInt());
    ui->comboAlgo->setCurrentText(config()->get("CRYPTO/cryptoAlgo").toString());
    ui->editName->setText(config()->get("userName").toString());
    ui->spinBox_clip->setValue(config()->get("SECURITY/clearclipboardtimeout").toInt());
    ui->checkBox_empty->setChecked(config()->get("SECURITY/clearclipboard").toBool());
}

void ConfigDialog::saveSettings()
{
    config()->set("extension",                         ui->editExtension->text());
    config()->set("CRYPTO/argonMemory",                ui->comboMemory->currentIndex());
    config()->set("CRYPTO/argonItr",                   ui->comboOps->currentIndex());
    config()->set("CRYPTO/cryptoAlgo",                 ui->comboAlgo->currentText());
    config()->set("userName",                          ui->editName->text());
    config()->set("SECURITY/clearclipboardtimeout",    ui->spinBox_clip->value());
    config()->set("SECURITY/clearclipboard",           ui->checkBox_empty->isChecked());
}

