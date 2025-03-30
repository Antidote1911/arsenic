/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
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

#include "Translator.h"

#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QTranslator>

#include "Config.h"

/**
 * Install all KeePassXC and Qt translators.
 */
void Translator::installTranslators()
{
    QStringList languages;
    QString languageSetting = config()->get(Config::GUI_Language).toString();
    if (languageSetting.isEmpty() || languageSetting == "system") {
        // NOTE: this is a workaround for the terrible way Qt loads languages
        // using the QLocale::uiLanguages() approach. Instead, we search each
        // language and all country variants in order before moving to the next.
        QLocale locale;
        languages = locale.uiLanguages();
    }
    else {
        languages << languageSetting;
    }

    // Always try to load english last
    languages << "en_US";

    QString path;

#ifdef Q_OS_WIN
    path = QApplication::applicationDirPath();
    path.append("/languages");
#endif
#ifdef Q_OS_UNIX
    path = "/usr/share/arsenic/languages/";
#endif

    installQtTranslator(languages, path);
    if (!installTranslator(languages, path)) {
        // couldn't load configured language or fallback
        qWarning("Couldn't load translations.");
    }
}

/**
 * Install KeePassXC translator.
 *
 * @param languages priority-ordered list of languages
 * @param path absolute search path
 * @return true on success
 */
bool Translator::installTranslator(const QStringList& languages, const QString& path)
{
    for (const auto& language : languages) {
        QLocale locale(language);
        std::unique_ptr<QTranslator> translator = std::make_unique<QTranslator>();
        //QScopedPointer<QTranslator> translator(new QTranslator(qApp));
        if (translator->load(locale, "arsenic_", "", path)) {
            return QCoreApplication::installTranslator(translator.release());
        }
    }

    return false;
}

/**
 * Install Qt5 base translator from the specified local search path or the default system path
 * if no qtbase_* translations were found at the local path.
 *
 * @param languages priority-ordered list of languages
 * @param path absolute search path
 * @return true on success
 */
bool Translator::installQtTranslator(const QStringList& languages, const QString& path)
{
    for (const auto& language : languages) {
        QLocale locale(language);
        std::unique_ptr<QTranslator> qtTranslator = std::make_unique<QTranslator>();
        if (qtTranslator->load(locale, "qtbase_", "", path)) {
            return QCoreApplication::installTranslator(qtTranslator.release());
        }
        else if (qtTranslator->load(locale, "qtbase_", "", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            return QCoreApplication::installTranslator(qtTranslator.release());
        }
    }
    return false;
}

/**
 * @return list of pairs of available language codes and names
 */
QList<QPair<QString, QString>> Translator::availableLanguages()
{
    QList<QPair<QString, QString>> languages;
    languages.append(QPair<QString, QString>("system", "System default"));

    QRegularExpression regExp("^arsenic_([a-zA-Z_]+)\\.qm$", QRegularExpression::CaseInsensitiveOption);

    QString path;

#ifdef Q_OS_WIN
    path = QApplication::applicationDirPath();
    path.append("/languages");
#endif
#ifdef Q_OS_UNIX
    path = "/usr/share/arsenic/languages/";
#endif

    const QStringList fileList = QDir(path).entryList();
    for (const QString& filename : fileList) {
        QRegularExpressionMatch match = regExp.match(filename);
        if (match.hasMatch()) {
            QString langcode = match.captured(1);
            if (langcode == "en") {
                continue;
            }

            QLocale locale(langcode);
            QString languageStr = QLocale::languageToString(locale.language());
            if (langcode == "la") {
                // langcode "la" (Latin) is translated into "C" by QLocale::languageToString()
                languageStr = "Latin";
            }
            if (langcode.contains("_")) {
                languageStr += QString(" (%1)").arg(QLocale::territoryToString(locale.territory()));
            }

            QPair<QString, QString> language(langcode, languageStr);
            languages.append(language);
        }
    }

    return languages;
}
