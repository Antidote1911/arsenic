#include "configDialog.h"
#include "Config.h"
#include "Translator.h"
#include "ui_configDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent), m_ui(new Ui::ConfigDialog)
{
    m_ui->setupUi(this);
    connect(this, SIGNAL(accepted()), SLOT(saveSettings()));
    // connect(this, SIGNAL(rejected()), SLOT(reject()));
    connect(m_ui->checkBox_empty, SIGNAL(toggled(bool)), m_ui->spinBox_clip, SLOT(setEnabled(bool)));
    loadSettings();
}

ConfigDialog::~ConfigDialog() {}

void ConfigDialog::loadSettings()
{
    if (config()->hasAccessError()) {
        QString warn_text = QString(tr("Access error for config file %1").arg(config()->getFileName()));
        QMessageBox::warning(this, tr("Could not load configuration"), warn_text);
    }
    m_ui->languageComboBox->clear();
    QList<QPair<QString, QString>> languages = Translator::availableLanguages();
    for (const auto& language : languages) {
        m_ui->languageComboBox->addItem(language.second, language.first);
    }
    int defaultIndex = m_ui->languageComboBox->findData(config()->get(Config::GUI_Language));
    if (defaultIndex > 0) {
        m_ui->languageComboBox->setCurrentIndex(defaultIndex);
    }

    m_ui->spinBox_clip->setEnabled(config()->get(Config::SECURITY_clearclipboard).toBool());
    m_ui->comboMemory->setCurrentIndex(config()->get(Config::CRYPTO_argonMemory).toInt());
    m_ui->comboOps->setCurrentIndex(config()->get(Config::CRYPTO_argonItr).toInt());
    m_ui->spinBox_clip->setValue(config()->get(Config::SECURITY_clearclipboardtimeout).toInt());
    m_ui->checkBox_empty->setChecked(config()->get(Config::SECURITY_clearclipboard).toBool());
    m_ui->checkAddEncrypted->setChecked(config()->get(Config::GUI_AddEncrypted).toBool());
    m_ui->comboAlgo->setCurrentText(config()->get(Config::CRYPTO_algorithm).toString());
}

void ConfigDialog::saveSettings()
{
    config()->set(Config::CRYPTO_argonMemory, m_ui->comboMemory->currentIndex());
    config()->set(Config::CRYPTO_argonItr, m_ui->comboOps->currentIndex());
    config()->set(Config::SECURITY_clearclipboardtimeout, m_ui->spinBox_clip->value());
    config()->set(Config::SECURITY_clearclipboard, m_ui->checkBox_empty->isChecked());
    config()->set(Config::GUI_AddEncrypted, m_ui->checkAddEncrypted->isChecked());
    config()->set(Config::GUI_Language, m_ui->languageComboBox->currentData().toString());
    config()->set(Config::CRYPTO_algorithm, m_ui->comboAlgo->currentText());
}
