/*
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "passwordGeneratorDialog.h"
#include "ui_passwordGeneratorDialog.h"

#include <QDir>
#include <QKeyEvent>
#include <QLineEdit>

#include "Config.h"
#include "clipboard.h"
#include "passwordGenerator.h"

PasswordGeneratorDialog::PasswordGeneratorDialog(QDialog *parent)
    : QDialog(parent), m_updatingSpinBox(false), m_passwordGenerator(new PasswordGenerator()), m_ui(new Ui::PasswordGeneratorDialog())
{
    m_ui->setupUi(this);

    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updateButtonsEnabled(QString)));
    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updatePasswordStrength(QString)));
    connect(m_ui->togglePasswordButton, SIGNAL(toggled(bool)), SLOT(setPasswordVisible(bool)));
    connect(m_ui->buttonSimpleMode, SIGNAL(clicked()), SLOT(selectSimpleMode()));
    connect(m_ui->buttonAdvancedMode, SIGNAL(clicked()), SLOT(selectAdvancedMode()));
    connect(m_ui->buttonAddHex, SIGNAL(clicked()), SLOT(excludeHexChars()));
    connect(m_ui->editExcludedChars, SIGNAL(textChanged(QString)), SLOT(updateGenerator()));
    connect(m_ui->buttonApply, SIGNAL(clicked()), SLOT(applyPassword()));
    connect(m_ui->buttonCopy, SIGNAL(clicked()), SLOT(copyPassword()));
    connect(m_ui->buttonGenerate, SIGNAL(clicked()), SLOT(regeneratePassword()));

    connect(m_ui->sliderLength, SIGNAL(valueChanged(int)), SLOT(passwordSliderMoved()));
    connect(m_ui->spinBoxLength, SIGNAL(valueChanged(int)), SLOT(passwordSpinBoxChanged()));

    connect(m_ui->optionButtons, SIGNAL(buttonClicked(int)), SLOT(updateGenerator()));

    // set font size of password quality and entropy labels dynamically to 80% of
    // the default font size, but make it no smaller than 8pt
    QFont defaultFont;
    int smallerSize = static_cast<int>(defaultFont.pointSize() * 0.8f);
    if (smallerSize >= 8) {
        defaultFont.setPointSize(smallerSize);
        m_ui->entropyLabel->setFont(defaultFont);
        m_ui->strengthLabel->setFont(defaultFont);
    }

    loadSettings();
}

PasswordGeneratorDialog::~PasswordGeneratorDialog() {}

void PasswordGeneratorDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reset();
}

void PasswordGeneratorDialog::loadSettings()
{
    // Password config
    m_ui->checkBoxLower->setChecked(config()->get(Config::PasswordGenerator_LowerCase).toBool());
    m_ui->checkBoxLowerAdv->setChecked(config()->get(Config::PasswordGenerator_LowerCase).toBool());
    m_ui->checkBoxUpper->setChecked(config()->get(Config::PasswordGenerator_UpperCase).toBool());
    m_ui->checkBoxUpperAdv->setChecked(config()->get(Config::PasswordGenerator_UpperCase).toBool());
    m_ui->checkBoxNumbers->setChecked(config()->get(Config::PasswordGenerator_Numbers).toBool());
    m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_SpecialChars).toBool());
    m_ui->checkBoxNumbersAdv->setChecked(config()->get(Config::PasswordGenerator_Numbers).toBool());
    m_ui->advancedBar->setVisible(config()->get(Config::PasswordGenerator_AdvancedMode).toBool());
    m_ui->excludedChars->setVisible(config()->get(Config::PasswordGenerator_AdvancedMode).toBool());
    m_ui->checkBoxExcludeAlike->setVisible(config()->get(Config::PasswordGenerator_ExcludeAlike).toBool());
    m_ui->checkBoxEnsureEvery->setVisible(config()->get(Config::PasswordGenerator_EnsureEvery).toBool());
    m_ui->editExcludedChars->setText(config()->get(Config::PasswordGenerator_ExcludedChars).toString());

    m_ui->simpleBar->setVisible(!(config()->get(Config::PasswordGenerator_AdvancedMode).toBool()));
    m_ui->checkBoxBraces->setChecked(config()->get(Config::PasswordGenerator_Braces).toBool());
    m_ui->checkBoxQuotes->setChecked(config()->get(Config::PasswordGenerator_Quotes).toBool());
    m_ui->checkBoxPunctuation->setChecked(config()->get(Config::PasswordGenerator_Punctuation).toBool());
    m_ui->checkBoxDashes->setChecked(config()->get(Config::PasswordGenerator_Dashes).toBool());
    m_ui->checkBoxMath->setChecked(config()->get(Config::PasswordGenerator_Math).toBool());
    m_ui->checkBoxLogograms->setChecked(config()->get(Config::PasswordGenerator_Logograms).toBool());
    m_ui->checkBoxExtASCII->setChecked(config()->get(Config::PasswordGenerator_EASCII).toBool());
    m_ui->checkBoxExtASCIIAdv->setChecked(config()->get(Config::PasswordGenerator_EASCII).toBool());
    m_ui->checkBoxExcludeAlike->setChecked(config()->get(Config::PasswordGenerator_ExcludeAlike).toBool());
    m_ui->checkBoxEnsureEvery->setChecked(config()->get(Config::PasswordGenerator_EnsureEvery).toBool());
    m_ui->spinBoxLength->setValue(config()->get(Config::PasswordGenerator_Length).toInt());
}

void PasswordGeneratorDialog::saveSettings()
{
    // Password config
    if (m_ui->simpleBar->isVisible()) {
        config()->set(Config::PasswordGenerator_LowerCase, m_ui->checkBoxLower->isChecked());
        config()->set(Config::PasswordGenerator_UpperCase, m_ui->checkBoxUpper->isChecked());
        config()->set(Config::PasswordGenerator_Numbers, m_ui->checkBoxNumbers->isChecked());
        config()->set(Config::PasswordGenerator_EASCII, m_ui->checkBoxExtASCII->isChecked());
    }
    else {
        config()->set(Config::PasswordGenerator_LowerCase, m_ui->checkBoxLowerAdv->isChecked());
        config()->set(Config::PasswordGenerator_UpperCase, m_ui->checkBoxUpperAdv->isChecked());
        config()->set(Config::PasswordGenerator_Numbers, m_ui->checkBoxNumbersAdv->isChecked());
        config()->set(Config::PasswordGenerator_EASCII, m_ui->checkBoxExtASCIIAdv->isChecked());
    }
    config()->set(Config::PasswordGenerator_AdvancedMode, m_ui->advancedBar->isVisible());
    config()->set(Config::PasswordGenerator_SpecialChars, m_ui->checkBoxSpecialChars->isChecked());
    config()->set(Config::PasswordGenerator_Braces, m_ui->checkBoxBraces->isChecked());
    config()->set(Config::PasswordGenerator_Punctuation, m_ui->checkBoxPunctuation->isChecked());
    config()->set(Config::PasswordGenerator_Quotes, m_ui->checkBoxQuotes->isChecked());
    config()->set(Config::PasswordGenerator_Dashes, m_ui->checkBoxDashes->isChecked());
    config()->set(Config::PasswordGenerator_Math, m_ui->checkBoxMath->isChecked());
    config()->set(Config::PasswordGenerator_Logograms, m_ui->checkBoxLogograms->isChecked());
    config()->set(Config::PasswordGenerator_ExcludedChars, m_ui->editExcludedChars->text());
    config()->set(Config::PasswordGenerator_ExcludeAlike, m_ui->checkBoxExcludeAlike->isChecked());
    config()->set(Config::PasswordGenerator_EnsureEvery, m_ui->checkBoxEnsureEvery->isChecked());
    config()->set(Config::PasswordGenerator_Length, m_ui->spinBoxLength->value());
}

void PasswordGeneratorDialog::reset(int length)
{
    m_ui->editNewPassword->setText("");
    if (length > 0)
        m_ui->spinBoxLength->setValue(length);
    else
        m_ui->spinBoxLength->setValue(config()->get(Config::PasswordGenerator_Length).toInt());

    setStandaloneMode(false);
    setPasswordVisible(config()->get(Config::GUI_showPassword).toBool());
    updateGenerator();
}

void PasswordGeneratorDialog::setStandaloneMode(bool standalone)
{
    m_standalone = standalone;
    if (standalone) {
        m_ui->buttonApply->setText(tr("Close"));
        setPasswordVisible(true);
    }
    else
        m_ui->buttonApply->setText(tr("Accept"));
}

QString PasswordGeneratorDialog::getGeneratedPassword()
{
    return (m_ui->editNewPassword->text());
}

void PasswordGeneratorDialog::keyPressEvent(QKeyEvent *e)
{
    if ((e->key() == Qt::Key_Escape) && m_standalone)
        emit dialogTerminated();
    else
        e->ignore();
}

void PasswordGeneratorDialog::regeneratePassword()
{
    if (m_passwordGenerator->isValid()) {
        QString password = m_passwordGenerator->generatePassword();
        m_ui->editNewPassword->setText(password);
        updatePasswordStrength(password);
    }
}

void PasswordGeneratorDialog::updateButtonsEnabled(const QString &password)
{
    if (!m_standalone) m_ui->buttonApply->setEnabled(!password.isEmpty());
    m_ui->buttonCopy->setEnabled(!password.isEmpty());
}

void PasswordGeneratorDialog::updatePasswordStrength(const QString &password)
{
    double entropy = 0.0;

    entropy = m_passwordGenerator->estimateEntropy(password);

    m_ui->entropyLabel->setText(tr("Entropy: %1 bit").arg(QString::number(entropy, 'f', 2)));

    if (entropy > m_ui->entropyProgressBar->maximum()) entropy = m_ui->entropyProgressBar->maximum();
    m_ui->entropyProgressBar->setValue(entropy);

    colorStrengthIndicator(entropy);
}

void PasswordGeneratorDialog::applyPassword()
{
    saveSettings();
    emit appliedPassword(m_ui->editNewPassword->text());
    emit dialogTerminated();
}

void PasswordGeneratorDialog::copyPassword()
{
    clipboard()->setText(m_ui->editNewPassword->text());
}

void PasswordGeneratorDialog::passwordSliderMoved()
{
    if (m_updatingSpinBox) return;

    m_ui->spinBoxLength->setValue(m_ui->sliderLength->value());

    updateGenerator();
}

void PasswordGeneratorDialog::passwordSpinBoxChanged()
{
    if (m_updatingSpinBox) return;

    // Interlock so that we don't update twice - this causes issues as the spinbox can go higher than slider
    m_updatingSpinBox = true;

    m_ui->sliderLength->setValue(m_ui->spinBoxLength->value());

    m_updatingSpinBox = false;

    updateGenerator();
}

void PasswordGeneratorDialog::setPasswordVisible(bool visible)
{
    if (visible) {
        m_ui->togglePasswordButton->setIcon(QIcon(":/pixmaps/password-show-on.svg"));
        m_ui->editNewPassword->setEchoMode(QLineEdit::Normal);
    }
    else {
        m_ui->togglePasswordButton->setIcon(QIcon(":/pixmaps/password-show-off.svg"));
        m_ui->editNewPassword->setEchoMode(QLineEdit::Password);
    }
}

bool PasswordGeneratorDialog::isPasswordVisible() const
{
    return (m_ui->togglePasswordButton->isChecked());
}

void PasswordGeneratorDialog::selectSimpleMode()
{
    m_ui->advancedBar->hide();
    m_ui->excludedChars->hide();
    m_ui->checkBoxExcludeAlike->hide();
    m_ui->checkBoxEnsureEvery->hide();
    m_ui->checkBoxUpper->setChecked(m_ui->checkBoxUpperAdv->isChecked());
    m_ui->checkBoxLower->setChecked(m_ui->checkBoxLowerAdv->isChecked());
    m_ui->checkBoxNumbers->setChecked(m_ui->checkBoxNumbersAdv->isChecked());
    m_ui->checkBoxSpecialChars->setChecked(m_ui->checkBoxBraces->isChecked() | m_ui->checkBoxPunctuation->isChecked() | m_ui->checkBoxQuotes->isChecked() |
                                           m_ui->checkBoxMath->isChecked() | m_ui->checkBoxDashes->isChecked() | m_ui->checkBoxLogograms->isChecked());
    m_ui->checkBoxExtASCII->setChecked(m_ui->checkBoxExtASCIIAdv->isChecked());
    m_ui->simpleBar->show();
}

/*******************************************************************************
 *
 *******************************************************************************/
void PasswordGeneratorDialog::selectAdvancedMode()
{
    m_ui->simpleBar->hide();
    m_ui->checkBoxUpperAdv->setChecked(m_ui->checkBoxUpper->isChecked());
    m_ui->checkBoxLowerAdv->setChecked(m_ui->checkBoxLower->isChecked());
    m_ui->checkBoxNumbersAdv->setChecked(m_ui->checkBoxNumbers->isChecked());
    m_ui->checkBoxBraces->setChecked(m_ui->checkBoxSpecialChars->isChecked());
    m_ui->checkBoxPunctuation->setChecked(m_ui->checkBoxSpecialChars->isChecked());
    m_ui->checkBoxQuotes->setChecked(m_ui->checkBoxSpecialChars->isChecked());
    m_ui->checkBoxMath->setChecked(m_ui->checkBoxSpecialChars->isChecked());
    m_ui->checkBoxDashes->setChecked(m_ui->checkBoxSpecialChars->isChecked());
    m_ui->checkBoxLogograms->setChecked(m_ui->checkBoxSpecialChars->isChecked());
    m_ui->checkBoxExtASCIIAdv->setChecked(m_ui->checkBoxExtASCII->isChecked());
    m_ui->advancedBar->show();
    m_ui->excludedChars->show();
    m_ui->checkBoxExcludeAlike->show();
    m_ui->checkBoxEnsureEvery->show();
}

void PasswordGeneratorDialog::excludeHexChars()
{
    m_ui->editExcludedChars->setText("GHIJKLMNOPQRSTUVWXYZghijklmnopqrstuvwxyz");
}

void PasswordGeneratorDialog::colorStrengthIndicator(double entropy)
{
    // Take the existing stylesheet and convert the text and background color to arguments
    QString style = m_ui->entropyProgressBar->styleSheet();
    QRegularExpression re("(QProgressBar::chunk\\s*\\{.*?background-color:)[^;]+;",
                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    style.replace(re, "\\1 %1;");

    // Set the color and background based on entropy
    // colors are taking from the KDE breeze palette
    // <https://community.kde.org/KDE_Visual_Design_Group/HIG/Color>
    if (entropy < 40) {
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#c0392b"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Poor", "Password quality")));
    }
    else if ((entropy >= 40) && (entropy < 65)) {
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#f39c1f"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Weak", "Password quality")));
    }
    else if ((entropy >= 65) && (entropy < 100)) {
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#11d116"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Good", "Password quality")));
    }
    else {
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#27ae60"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Excellent", "Password quality")));
    }
}

PasswordGenerator::CharClasses PasswordGeneratorDialog::charClasses()
{
    PasswordGenerator::CharClasses classes;

    if (m_ui->simpleBar->isVisible()) {
        if (m_ui->checkBoxLower->isChecked()) classes |= PasswordGenerator::LowerLetters;

        if (m_ui->checkBoxUpper->isChecked()) classes |= PasswordGenerator::UpperLetters;

        if (m_ui->checkBoxNumbers->isChecked()) classes |= PasswordGenerator::Numbers;

        if (m_ui->checkBoxSpecialChars->isChecked()) classes |= PasswordGenerator::SpecialCharacters;

        if (m_ui->checkBoxExtASCII->isChecked()) classes |= PasswordGenerator::EASCII;
    }
    else {
        if (m_ui->checkBoxLowerAdv->isChecked()) classes |= PasswordGenerator::LowerLetters;

        if (m_ui->checkBoxUpperAdv->isChecked()) classes |= PasswordGenerator::UpperLetters;

        if (m_ui->checkBoxNumbersAdv->isChecked()) classes |= PasswordGenerator::Numbers;

        if (m_ui->checkBoxBraces->isChecked()) classes |= PasswordGenerator::Braces;

        if (m_ui->checkBoxPunctuation->isChecked()) classes |= PasswordGenerator::Punctuation;

        if (m_ui->checkBoxQuotes->isChecked()) classes |= PasswordGenerator::Quotes;

        if (m_ui->checkBoxDashes->isChecked()) classes |= PasswordGenerator::Dashes;

        if (m_ui->checkBoxMath->isChecked()) classes |= PasswordGenerator::Math;

        if (m_ui->checkBoxLogograms->isChecked()) classes |= PasswordGenerator::Logograms;

        if (m_ui->checkBoxExtASCIIAdv->isChecked()) classes |= PasswordGenerator::EASCII;
    }

    return (classes);
}

PasswordGenerator::GeneratorFlags PasswordGeneratorDialog::generatorFlags()
{
    PasswordGenerator::GeneratorFlags flags;

    if (m_ui->checkBoxExcludeAlike->isChecked()) flags |= PasswordGenerator::ExcludeLookAlike;

    if (m_ui->checkBoxEnsureEvery->isChecked()) flags |= PasswordGenerator::CharFromEveryGroup;

    return (flags);
}

void PasswordGeneratorDialog::updateGenerator()
{
    PasswordGenerator::CharClasses classes  = charClasses();
    PasswordGenerator::GeneratorFlags flags = generatorFlags();

    int minLength = 0;
    if (flags.testFlag(PasswordGenerator::CharFromEveryGroup)) {
        if (classes.testFlag(PasswordGenerator::LowerLetters)) minLength++;
        if (classes.testFlag(PasswordGenerator::UpperLetters)) minLength++;
        if (classes.testFlag(PasswordGenerator::Numbers)) minLength++;
        if (classes.testFlag(PasswordGenerator::Braces)) minLength++;
        if (classes.testFlag(PasswordGenerator::Punctuation)) minLength++;
        if (classes.testFlag(PasswordGenerator::Quotes)) minLength++;
        if (classes.testFlag(PasswordGenerator::Dashes)) minLength++;
        if (classes.testFlag(PasswordGenerator::Math)) minLength++;
        if (classes.testFlag(PasswordGenerator::Logograms)) minLength++;
        if (classes.testFlag(PasswordGenerator::EASCII)) minLength++;
    }
    minLength = qMax(minLength, 1);

    if (m_ui->spinBoxLength->value() < minLength) {
        m_updatingSpinBox = true;
        m_ui->spinBoxLength->setValue(minLength);
        m_ui->sliderLength->setValue(minLength);
        m_updatingSpinBox = false;
    }

    m_ui->spinBoxLength->setMinimum(minLength);
    m_ui->sliderLength->setMinimum(minLength);

    m_passwordGenerator->setLength(m_ui->spinBoxLength->value());
    m_passwordGenerator->setCharClasses(classes);
    if (m_ui->simpleBar->isVisible())
        m_passwordGenerator->setExcludedChars("");
    else
        m_passwordGenerator->setExcludedChars(m_ui->editExcludedChars->text());
    m_passwordGenerator->setFlags(flags);

    if (m_passwordGenerator->isValid())
        m_ui->buttonGenerate->setEnabled(true);
    else
        m_ui->buttonGenerate->setEnabled(false);

    regeneratePassword();
}
