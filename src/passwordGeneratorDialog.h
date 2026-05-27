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

#ifndef KEEPASSX_PASSWORDGENERATORWIDGET_H
#define KEEPASSX_PASSWORDGENERATORWIDGET_H

#include <QComboBox>
#include <QDialog>
#include <QTimer>


#include "arscore/PasswordGenerator.h"

namespace Ui
{
    class PasswordGeneratorWidget;
}

class PasswordGenerator;


class PasswordGeneratorWidget : public QDialog
{
    Q_OBJECT

public:

    explicit PasswordGeneratorWidget(QDialog* parent = nullptr);
    ~PasswordGeneratorWidget() override;

    void loadSettings();
    void saveSettings();
    void setPasswordLength(int length);
    void setStandaloneMode(bool standalone);
    bool isPasswordGenerated() const;

signals:
    void appliedPassword(const QString& password);
    void dialogTerminated();
    void closed();

public slots:
    void regeneratePassword();
    void applyPassword();
    void copyPassword();
    void closeApp();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void updateButtonsEnabled(const QString& password);
    void updatePasswordStrength();
    void updatePasswordLengthLabel(const QString& password);
    void setAdvancedMode(bool advanced);
    void setPasswordVisibility(bool visible);
    void excludeHexChars();


    void passwordLengthChanged(int length);

    void updateGenerator();

private:
    void saveWindowInformation();    
    bool m_passwordGenerated = false;
    bool m_passwordVisible;
    bool m_standalone = false;
    PasswordGenerator::CharClasses charClasses();
    PasswordGenerator::GeneratorFlags generatorFlags();

    const QScopedPointer<PasswordGenerator> m_passwordGenerator;
    const QScopedPointer<Ui::PasswordGeneratorWidget> m_ui;
};

#endif // KEEPASSX_PASSWORDGENERATORWIDGET_H
