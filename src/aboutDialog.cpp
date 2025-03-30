/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "aboutDialog.h"
#include "arscore/utils.h"
#include "Config.h"
#include "loghtml.h"
#include "ui_aboutDialog.h"

#include <QClipboard>
#include <QFile>

static const QString aboutMaintainers = R"(
<p><ul>
    <li>Fabrice Corraire (<a href="https://github.com/Antidote1911">Antidote1911</a>)</li>
    <li>Lilou Corraire (English traduction)</li>
    <li>Sony Legrand (English traduction)</li>
</ul></p>
)";

static const QString aboutContributors = R"(
<h3>VIP Patreon Supporters:</h3>
<ul>
    <li>Lilou Corraire</li>
    <li>Sony Legrand</li>

</ul>
<h3>Notable Code Contributions:</h3>
<ul>
    <li>Sony Legrand</li>
</ul>
<h3>Translations:</h3>
<ul>
    <li><strong>Français</strong>: Liloutronic</li>
</ul>
)";

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), m_ui(new Ui::AboutDialog)
{
    m_ui->setupUi(this);

    resize(minimumSize());
    setWindowFlags(Qt::Sheet);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_ui->nameLabel->setText(m_ui->nameLabel->text().replace("${VERSION}", m_const->APP_VERSION.toString()));
    m_ui->descriptionLabel->setText(m_ui->descriptionLabel->text().replace("${DESCRIPTION}", m_const->APP_DESCRIPTION));
    QFont nameLabelFont = m_ui->nameLabel->font();
    nameLabelFont.setPointSize(nameLabelFont.pointSize() + 4);
    m_ui->nameLabel->setFont(nameLabelFont);

    QString debugInfo;
    debugInfo.append(m_const->APP_LONG_NAME).append("\n");
    debugInfo.append("Qt ").append(QString::fromLocal8Bit(qVersion())).append("\n");
    debugInfo.append("Operating system: " + QSysInfo::prettyProductName()).append("\n");
    debugInfo.append("CPU architecture: " + QSysInfo::currentCpuArchitecture()).append("\n");
    debugInfo.append("Kernel: " + QSysInfo::kernelType()).append(QSysInfo::kernelVersion()).append("\n");
    debugInfo.append("Hostname: " + QSysInfo::machineHostName()).append("\n");
    debugInfo.append(m_const->BOTAN_VERSION).append("\n");
    debugInfo.append("Tmp folder: " + Utils::getTempPath()).append("\n");
    debugInfo.append("Config file: " + config()->getFileName()).append("\n");
    debugInfo.append("Log file: " + loghtml()->getPath()).append("\n");

    m_ui->iconLabel->setPixmap(qApp->windowIcon().pixmap(48));

    // QString debugInfo = Tools::debugInfo().append("\n").append(Crypto::debugInfo());
    m_ui->debugInfo->setPlainText(debugInfo);

    m_ui->maintainers->setText(aboutMaintainers);
    m_ui->contributors->setText(aboutContributors);

    QFile file("://about_files/gpl-3.0.html");
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);
    m_ui->gpllicenseEdit->setHtml(stream.readAll());
    file.close();

    QFile file2("://about_files/botan_license.txt");
    file2.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream2(&file2);
    m_ui->botanlicenseEdit->setText(stream2.readAll());
    file2.close();

    setAttribute(Qt::WA_DeleteOnClose);
    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(close()));
    connect(m_ui->copyToClipboard, SIGNAL(clicked()), SLOT(copyToClipboard()));
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::copyToClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(m_ui->debugInfo->toPlainText());
}
