/*
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2022 KeePassXC Team <team@keepassxc.org>
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
#include "arscore/PasswordHealth.h"
#include "ui_passwordGeneratorDialog.h"

#include <QMainWindow>
#include <QCloseEvent>
#include <QDir>
#include <QShortcut>
#include <QTimer>

#include "Config.h"
#include "Clipboard.h"




PasswordGeneratorWidget::PasswordGeneratorWidget(QDialog* parent)
    : QDialog(parent)
    , m_passwordGenerator(new PasswordGenerator())
    , m_ui(new Ui::PasswordGeneratorWidget())
{
    m_ui->setupUi(this);
    this->setWindowTitle("Shredder Password Generator");

    m_ui->buttonGenerate->setIcon(QIcon(":/pixmaps/refresh.svg"));
    m_ui->togglePasswordButton->setIcon(QIcon(":/pixmaps/password-show-on.svg"));
    m_ui->buttonCopy->setIcon(QIcon(":/pixmaps/clipboard.svg"));

    m_ui->buttonGenerate->setToolTip(
        tr("Regenerate password (%1)").arg(m_ui->buttonGenerate->shortcut().toString(QKeySequence::NativeText)));

    m_ui->buttonClose->setShortcut(Qt::Key_Escape);

    // Add two shortcuts to save the form CTRL+Enter and CTRL+S
    auto shortcut = new QShortcut(Qt::CTRL | Qt::Key_Return, this);
    connect(shortcut, &QShortcut::activated, this, [this] { applyPassword(); });
    shortcut = new QShortcut(Qt::CTRL | Qt::Key_S, this);
    connect(shortcut, &QShortcut::activated, this, [this] { applyPassword(); });

    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updateButtonsEnabled(QString)));
    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updatePasswordStrength()));
    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updatePasswordLengthLabel(QString)));
    connect(m_ui->buttonAdvancedMode, SIGNAL(toggled(bool)), SLOT(setAdvancedMode(bool)));
    connect(m_ui->buttonAddHex, SIGNAL(clicked()), SLOT(excludeHexChars()));
    connect(m_ui->editAdditionalChars, SIGNAL(textChanged(QString)), SLOT(updateGenerator()));
    connect(m_ui->editExcludedChars, SIGNAL(textChanged(QString)), SLOT(updateGenerator()));
    connect(m_ui->buttonCopy, SIGNAL(clicked()), SLOT(copyPassword()));
    connect(m_ui->buttonApply, SIGNAL(clicked()), SLOT(applyPassword()));
    connect(m_ui->buttonGenerate, SIGNAL(clicked()), SLOT(regeneratePassword()));
    connect(m_ui->buttonClose, SIGNAL(clicked()), SLOT(closeApp()));
    connect(m_ui->togglePasswordButton, SIGNAL(toggled(bool)), SLOT(setPasswordVisibility(bool)));

    connect(m_ui->sliderLength, SIGNAL(valueChanged(int)), SLOT(passwordLengthChanged(int)));
    connect(m_ui->spinBoxLength, SIGNAL(valueChanged(int)), SLOT(passwordLengthChanged(int)));

    connect(m_ui->optionButtons, SIGNAL(idClicked(int)), SLOT(updateGenerator()));

    restoreGeometry(config()->get(Config::GUI_MainWindowGeometry).toByteArray());

    // set font size of password quality, characters, and entropy labels dynamically to 80% of
    // the default font size, but make it no smaller than 8pt
    QFont defaultFont;
    auto smallerSize = static_cast<int>(defaultFont.pointSize() * 0.8f);
    if (smallerSize >= 8) {
        defaultFont.setPointSize(smallerSize);
        m_ui->entropyLabel->setFont(defaultFont);
        m_ui->strengthLabel->setFont(defaultFont);
        m_ui->passwordLengthLabel->setFont(defaultFont);
    }

    loadSettings();
}

PasswordGeneratorWidget::~PasswordGeneratorWidget() = default;

void PasswordGeneratorWidget::closeEvent(QCloseEvent* event)
{
    saveWindowInformation();
    // Emits closed signal when clicking X from title bar
    emit closed();
    QWidget::closeEvent(event);
}

void PasswordGeneratorWidget::closeApp()
{
    saveSettings();
    m_passwordGenerated = false;
    emit dialogTerminated();
}

void PasswordGeneratorWidget::setStandaloneMode(bool standalone)
{
    m_standalone = standalone;
    if (standalone) {
        m_ui->buttonApply->setText(tr("Close"));
    }
    else
        m_ui->buttonApply->setText(tr("Accept"));
}

void PasswordGeneratorWidget::loadSettings()
{

    m_ui->checkBoxLower->setChecked(config()->get(Config::PasswordGenerator_LowerCase).toBool());
    m_ui->checkBoxUpper->setChecked(config()->get(Config::PasswordGenerator_UpperCase).toBool());
    m_ui->checkBoxNumbers->setChecked(config()->get(Config::PasswordGenerator_Numbers).toBool());
    m_ui->editAdditionalChars->setText(config()->get(Config::PasswordGenerator_AdditionalChars).toString());
    m_ui->editExcludedChars->setText(config()->get(Config::PasswordGenerator_ExcludedChars).toString());

    bool advanced = config()->get(Config::PasswordGenerator_AdvancedMode).toBool();
    if (advanced) {
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_Logograms).toBool());
    } else {
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_SpecialChars).toBool());
    }

    m_ui->checkBoxBraces->setChecked(config()->get(Config::PasswordGenerator_Braces).toBool());
    m_ui->checkBoxQuotes->setChecked(config()->get(Config::PasswordGenerator_Quotes).toBool());
    m_ui->checkBoxPunctuation->setChecked(config()->get(Config::PasswordGenerator_Punctuation).toBool());
    m_ui->checkBoxDashes->setChecked(config()->get(Config::PasswordGenerator_Dashes).toBool());
    m_ui->checkBoxMath->setChecked(config()->get(Config::PasswordGenerator_Math).toBool());

    m_ui->checkBoxExtASCII->setChecked(config()->get(Config::PasswordGenerator_EASCII).toBool());
    m_ui->checkBoxExcludeAlike->setChecked(config()->get(Config::PasswordGenerator_ExcludeAlike).toBool());
    m_ui->checkBoxEnsureEvery->setChecked(config()->get(Config::PasswordGenerator_EnsureEvery).toBool());
    m_ui->spinBoxLength->setValue(config()->get(Config::PasswordGenerator_Length).toInt());

    m_ui->togglePasswordButton->setChecked(config()->get(Config::PasswordGenerator_ViewPassword).toBool());
    setPasswordVisibility(config()->get(Config::PasswordGenerator_ViewPassword).toBool());


    // Set advanced mode
    m_ui->buttonAdvancedMode->setChecked(advanced);
    setAdvancedMode(advanced);
    updateGenerator();
}

void PasswordGeneratorWidget::saveSettings()
{
    // Password config
    config()->set(Config::PasswordGenerator_ViewPassword, m_ui->togglePasswordButton->isChecked());
    config()->set(Config::PasswordGenerator_LowerCase, m_ui->checkBoxLower->isChecked());
    config()->set(Config::PasswordGenerator_UpperCase, m_ui->checkBoxUpper->isChecked());
    config()->set(Config::PasswordGenerator_Numbers, m_ui->checkBoxNumbers->isChecked());
    config()->set(Config::PasswordGenerator_EASCII, m_ui->checkBoxExtASCII->isChecked());

    config()->set(Config::PasswordGenerator_AdvancedMode, m_ui->buttonAdvancedMode->isChecked());
    if (m_ui->buttonAdvancedMode->isChecked()) {
        config()->set(Config::PasswordGenerator_Logograms, m_ui->checkBoxSpecialChars->isChecked());
    } else {
        config()->set(Config::PasswordGenerator_SpecialChars, m_ui->checkBoxSpecialChars->isChecked());
    }
    config()->set(Config::PasswordGenerator_Braces, m_ui->checkBoxBraces->isChecked());
    config()->set(Config::PasswordGenerator_Punctuation, m_ui->checkBoxPunctuation->isChecked());
    config()->set(Config::PasswordGenerator_Quotes, m_ui->checkBoxQuotes->isChecked());
    config()->set(Config::PasswordGenerator_Dashes, m_ui->checkBoxDashes->isChecked());
    config()->set(Config::PasswordGenerator_Math, m_ui->checkBoxMath->isChecked());

    config()->set(Config::PasswordGenerator_AdditionalChars, m_ui->editAdditionalChars->text());
    config()->set(Config::PasswordGenerator_ExcludedChars, m_ui->editExcludedChars->text());
    config()->set(Config::PasswordGenerator_ExcludeAlike, m_ui->checkBoxExcludeAlike->isChecked());
    config()->set(Config::PasswordGenerator_EnsureEvery, m_ui->checkBoxEnsureEvery->isChecked());
    config()->set(Config::PasswordGenerator_Length, m_ui->spinBoxLength->value());

}

void PasswordGeneratorWidget::setPasswordLength(int length)
{
    if (length > 0) {
        m_ui->spinBoxLength->setValue(length);
    } else {
        m_ui->spinBoxLength->setValue(config()->get(Config::PasswordGenerator_Length).toInt());
    }
}


void PasswordGeneratorWidget::regeneratePassword()
{

        if (m_passwordGenerator->isValid()) {
            m_ui->editNewPassword->setText(m_passwordGenerator->generatePassword());
        }

}

void PasswordGeneratorWidget::updateButtonsEnabled(const QString& password)
{
    m_ui->buttonCopy->setEnabled(!password.isEmpty());
}

void PasswordGeneratorWidget::updatePasswordStrength()
{

PasswordHealth passwordHealth(0);
        // Calculate the password / passphrase health

        passwordHealth = PasswordHealth(m_ui->editNewPassword->text());


    // Update the entropy text labels
    m_ui->entropyLabel->setText(tr("Entropy: %1 bit").arg(QString::number(passwordHealth.entropy(), 'f', 2)));
    m_ui->entropyProgressBar->setValue(std::min(int(passwordHealth.entropy()), m_ui->entropyProgressBar->maximum()));

    // Update the visual strength meter
    QString style = m_ui->entropyProgressBar->styleSheet();
    QRegularExpression re("(QProgressBar::chunk\\s*\\{.*?background-color:)[^;]+;",
                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    style.replace(re, "\\1 %1;");

    // Set the color and background based on entropy
    // colors are taking from the KDE breeze palette
    // <https://community.kde.org/KDE_Visual_Design_Group/HIG/Color>

    switch (passwordHealth.quality()) {
    case PasswordHealth::Quality::Bad:
    case PasswordHealth::Quality::Poor:
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#c0392b"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Poor", "Password quality")));
        break;

    case PasswordHealth::Quality::Weak:
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#f39c1f"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Weak", "Password quality")));
        break;

    case PasswordHealth::Quality::Good:
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#11d116"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Good", "Password quality")));
        break;

    case PasswordHealth::Quality::Excellent:
        m_ui->entropyProgressBar->setStyleSheet(style.arg("#27ae60"));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Excellent", "Password quality")));
        break;
    }


}

void PasswordGeneratorWidget::updatePasswordLengthLabel(const QString& password)
{
    m_ui->passwordLengthLabel->setText(tr("Characters: %1").arg(QString::number(password.length())));
}

void PasswordGeneratorWidget::applyPassword()
{
    saveSettings();
    m_passwordGenerated = true;
    emit appliedPassword(m_ui->editNewPassword->text());
    //emit closed();
    emit dialogTerminated();
}

void PasswordGeneratorWidget::copyPassword()
{
    clipboard()->setText(m_ui->editNewPassword->text());
}

void PasswordGeneratorWidget::passwordLengthChanged(int length)
{
    m_ui->spinBoxLength->blockSignals(true);
    m_ui->sliderLength->blockSignals(true);

    m_ui->spinBoxLength->setValue(length);
    m_ui->sliderLength->setValue(length);

    m_ui->spinBoxLength->blockSignals(false);
    m_ui->sliderLength->blockSignals(false);

    updateGenerator();
}

bool PasswordGeneratorWidget::isPasswordGenerated() const
{
    return m_passwordGenerated;
}

void PasswordGeneratorWidget::setPasswordVisibility(bool visible)
{
    if (visible) {
        m_ui->togglePasswordButton->setIcon(QIcon(":/pixmaps/password-show-on.svg"));
        m_ui->editNewPassword->setEchoMode(QLineEdit::Normal);
        m_passwordVisible=true;
    }
    else {
        m_ui->togglePasswordButton->setIcon(QIcon(":/pixmaps/password-show-off.svg"));
        m_ui->editNewPassword->setEchoMode(QLineEdit::Password);
        m_passwordVisible=false;
    }
}

void PasswordGeneratorWidget::setAdvancedMode(bool advanced)
{
    saveSettings();

    if (advanced) {
        m_ui->checkBoxSpecialChars->setText("# $ % && @ ^ ` ~");
        m_ui->checkBoxSpecialChars->setToolTip(tr("Logograms"));
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_Logograms).toBool());
    } else {
        m_ui->checkBoxSpecialChars->setText("/ * + && …");
        m_ui->checkBoxSpecialChars->setToolTip(tr("Special Characters"));
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_SpecialChars).toBool());
    }

    m_ui->advancedContainer->setVisible(advanced);
    m_ui->checkBoxBraces->setVisible(advanced);
    m_ui->checkBoxPunctuation->setVisible(advanced);
    m_ui->checkBoxQuotes->setVisible(advanced);
    m_ui->checkBoxMath->setVisible(advanced);
    m_ui->checkBoxDashes->setVisible(advanced);
    QTimer::singleShot(50, this, [this] { adjustSize(); });
}

void PasswordGeneratorWidget::excludeHexChars()
{
    m_ui->editExcludedChars->setText("GHIJKLMNOPQRSTUVWXYZ");
    m_ui->checkBoxNumbers->setChecked(true);
    m_ui->checkBoxUpper->setChecked(true);

    m_ui->checkBoxLower->setChecked(false);
    m_ui->checkBoxSpecialChars->setChecked(false);
    m_ui->checkBoxExtASCII->setChecked(false);
    m_ui->checkBoxPunctuation->setChecked(false);
    m_ui->checkBoxQuotes->setChecked(false);
    m_ui->checkBoxDashes->setChecked(false);
    m_ui->checkBoxMath->setChecked(false);
    m_ui->checkBoxBraces->setChecked(false);

    updateGenerator();
}

PasswordGenerator::CharClasses PasswordGeneratorWidget::charClasses()
{
    PasswordGenerator::CharClasses classes;

    if (m_ui->checkBoxLower->isChecked()) {
        classes |= PasswordGenerator::LowerLetters;
    }

    if (m_ui->checkBoxUpper->isChecked()) {
        classes |= PasswordGenerator::UpperLetters;
    }

    if (m_ui->checkBoxNumbers->isChecked()) {
        classes |= PasswordGenerator::Numbers;
    }

    if (m_ui->checkBoxExtASCII->isChecked()) {
        classes |= PasswordGenerator::EASCII;
    }

    if (!m_ui->buttonAdvancedMode->isChecked()) {
        if (m_ui->checkBoxSpecialChars->isChecked()) {
            classes |= PasswordGenerator::SpecialCharacters;
        }
    } else {
        if (m_ui->checkBoxBraces->isChecked()) {
            classes |= PasswordGenerator::Braces;
        }

        if (m_ui->checkBoxPunctuation->isChecked()) {
            classes |= PasswordGenerator::Punctuation;
        }

        if (m_ui->checkBoxQuotes->isChecked()) {
            classes |= PasswordGenerator::Quotes;
        }

        if (m_ui->checkBoxDashes->isChecked()) {
            classes |= PasswordGenerator::Dashes;
        }

        if (m_ui->checkBoxMath->isChecked()) {
            classes |= PasswordGenerator::Math;
        }

        if (m_ui->checkBoxSpecialChars->isChecked()) {
            classes |= PasswordGenerator::Logograms;
        }
    }

    return classes;
}

PasswordGenerator::GeneratorFlags PasswordGeneratorWidget::generatorFlags()
{
    PasswordGenerator::GeneratorFlags flags;

    if (m_ui->buttonAdvancedMode->isChecked()) {
        if (m_ui->checkBoxExcludeAlike->isChecked()) {
            flags |= PasswordGenerator::ExcludeLookAlike;
        }

        if (m_ui->checkBoxEnsureEvery->isChecked()) {
            flags |= PasswordGenerator::CharFromEveryGroup;
        }
    }

    return flags;
}

void PasswordGeneratorWidget::updateGenerator()
{

        auto classes = charClasses();
        auto flags = generatorFlags();

        m_passwordGenerator->setLength(m_ui->spinBoxLength->value());
        if (m_ui->buttonAdvancedMode->isChecked()) {
            m_passwordGenerator->setCharClasses(classes);
            m_passwordGenerator->setCustomCharacterSet(m_ui->editAdditionalChars->text());
            m_passwordGenerator->setExcludedCharacterSet(m_ui->editExcludedChars->text());
        } else {
            m_passwordGenerator->setCharClasses(classes);
        }
        m_passwordGenerator->setFlags(flags);

        if (m_passwordGenerator->isValid()) {
            m_ui->buttonGenerate->setEnabled(true);
        } else {
            m_ui->buttonGenerate->setEnabled(false);
        }

    regeneratePassword();
}

void PasswordGeneratorWidget::saveWindowInformation()
{
    saveSettings();
    config()->set(Config::GUI_MainWindowGeometry, saveGeometry());
}
