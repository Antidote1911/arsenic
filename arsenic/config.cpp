/*
 * This file is part of Arsenic.
 *
 * Copyright (C) 2017 Corraire Fabrice <antidote1911@gmail.com>
 *
 * Arsenic is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * Arsenic is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Arsenic. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ui_config.h"
#include <iostream>
#include <QComboBox>
#include <QSettings>
#include <QLabel>
#include <QCheckBox>
#include "preferences/Constants.h"


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
