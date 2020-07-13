/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
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

#include "Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QSettings>
#include <QSize>
#include <QStandardPaths>
#include <QTemporaryFile>

#define CONFIG_VERSION 1
#define QS QStringLiteral

enum ConfigType {
    Local,
    Roaming
};

struct ConfigDirective {
    QString name;
    ConfigType type;
    QVariant defaultValue;
};

// clang-format off

/**
 * Map of legal config values, their type and default value.
 */
static const QHash<Config::ConfigKey, ConfigDirective> configStrings = {
    //arsenic

    {Config::GUI_showPassword, {QS("GUI/showPassword"), Roaming, true}},
    {Config::GUI_deleteFinished, {QS("GUI/deleteFinished"), Roaming, true}},
    {Config::GUI_Language, {QS("GUI/Language"), Roaming, QS("en")}},
    {Config::GUI_showToolbar, {QS("GUI/showToolbar"), Roaming, true}},
    {Config::GUI_lastDirectory, {QS("GUI/lastDirectory"), Roaming, QDir::homePath()}},
    {Config::GUI_currentIndexTab, {QS("GUI/currentIndexTab"), Roaming, 0}},
    {Config::GUI_AddEncrypted, {QS("GUI/AddEncrypted"), Roaming, true}},
    {Config::GUI_ApplicationTheme, {QS("GUI/ApplicationTheme"), Roaming, QS("auto")}},

    {Config::GUI_MainWindowGeometry, {QS("GUI/MainWindowGeometry"), Local, {}}},
    {Config::GUI_MainWindowState, {QS("GUI/MainWindowState"), Local, {}}},

    {Config::CRYPTO_argonMemory, {QS("CRYPTO/argonMemory"), Roaming, 0}},
    {Config::CRYPTO_argonItr, {QS("CRYPTO/argonItr"), Roaming, 0}},

    {Config::SECURITY_clearclipboard, {QS("SECURITY/clearclipboard"), Roaming, true}},
    {Config::SECURITY_clearclipboardtimeout, {QS("SECURITY/clearclipboardtimeout"), Roaming, 10}},

    // PasswordGenerator
    {Config::PasswordGenerator_LowerCase, {QS("PasswordGenerator/LowerCase"), Roaming, true}},
    {Config::PasswordGenerator_UpperCase, {QS("PasswordGenerator/UpperCase"), Roaming, true}},
    {Config::PasswordGenerator_Numbers, {QS("PasswordGenerator/Numbers"), Roaming, true}},
    {Config::PasswordGenerator_EASCII, {QS("PasswordGenerator/EASCII"), Roaming, false}},
    {Config::PasswordGenerator_AdvancedMode, {QS("PasswordGenerator/AdvancedMode"), Roaming, false}},
    {Config::PasswordGenerator_SpecialChars, {QS("PasswordGenerator/SpecialChars"), Roaming, true}},
    {Config::PasswordGenerator_Braces, {QS("PasswordGenerator/Braces"), Roaming, false}},
    {Config::PasswordGenerator_Punctuation, {QS("PasswordGenerator/Punctuation"), Roaming, false}},
    {Config::PasswordGenerator_Quotes, {QS("PasswordGenerator/Quotes"), Roaming, false}},
    {Config::PasswordGenerator_Dashes, {QS("PasswordGenerator/Dashes"), Roaming, false}},
    {Config::PasswordGenerator_Math, {QS("PasswordGenerator/Math"), Roaming, false}},
    {Config::PasswordGenerator_Logograms, {QS("PasswordGenerator/Logograms"), Roaming, false}},
    {Config::PasswordGenerator_AdditionalChars, {QS("PasswordGenerator/AdditionalChars"), Roaming, {}}},
    {Config::PasswordGenerator_ExcludedChars, {QS("PasswordGenerator/ExcludedChars"), Roaming, {}}},
    {Config::PasswordGenerator_ExcludeAlike, {QS("PasswordGenerator/ExcludeAlike"), Roaming, true}},
    {Config::PasswordGenerator_EnsureEvery, {QS("PasswordGenerator/EnsureEvery"), Roaming, true}},
    {Config::PasswordGenerator_Length, {QS("PasswordGenerator/Length"), Roaming, 20}},
    {Config::PasswordGenerator_WordCount, {QS("PasswordGenerator/WordCount"), Roaming, 7}},
    {Config::PasswordGenerator_WordSeparator, {QS("PasswordGenerator/WordSeparator"), Roaming, QS(" ")}},
    {Config::PasswordGenerator_WordList, {QS("PasswordGenerator/WordList"), Roaming, QS("eff_large.wordlist")}},
    {Config::PasswordGenerator_WordCase, {QS("PasswordGenerator/WordCase"), Roaming, 0}},
    {Config::PasswordGenerator_Type, {QS("PasswordGenerator/Type"), Roaming, 0}}};

// clang-format on

QPointer<Config> Config::m_instance(nullptr);

QVariant Config::get(ConfigKey key)
{
    auto cfg          = configStrings[key];
    auto defaultValue = configStrings[key].defaultValue;
    if (m_localSettings && cfg.type == Local) {
        return m_localSettings->value(cfg.name, defaultValue);
    }
    return m_settings->value(cfg.name, defaultValue);
}

bool Config::hasAccessError()
{
    return m_settings->status() & QSettings::AccessError;
}

QString Config::getFileName()
{
    return m_settings->fileName();
}

void Config::set(ConfigKey key, const QVariant& value)
{
    if (get(key) == value) {
        return;
    }

    auto cfg = configStrings[key];
    if (cfg.type == Local && m_localSettings) {
        m_localSettings->setValue(cfg.name, value);
    }
    else {
        m_settings->setValue(cfg.name, value);
    }

    emit changed(key);
}

void Config::remove(ConfigKey key)
{
    auto cfg = configStrings[key];
    if (cfg.type == Local && m_localSettings) {
        m_localSettings->remove(cfg.name);
    }
    else {
        m_settings->remove(cfg.name);
    }

    emit changed(key);
}

/**
 * Sync configuration with persistent storage.
 *
 * Usually, you don't need to call this method manually, but if you are writing
 * configurations after an emitted \link QCoreApplication::aboutToQuit() signal,
 * use it to guarantee your config values are persisted.
 */
void Config::sync()
{
    m_settings->sync();
    if (m_localSettings) {
        m_localSettings->sync();
    }
}

void Config::resetToDefaults()
{
    m_settings->clear();
    if (m_localSettings) {
        m_localSettings->clear();
    }
}

/**
 * Map of configuration file settings that are either deprecated, or have
 * had their name changed to their new config enum values.
 *
 * Set a value to Deleted to remove the setting.
 */
static const QHash<QString, Config::ConfigKey> deprecationMap = {

    {QS("generator/LowerCase"), Config::PasswordGenerator_LowerCase},
    {QS("generator/UpperCase"), Config::PasswordGenerator_UpperCase},
    {QS("generator/Numbers"), Config::PasswordGenerator_Numbers},
    {QS("generator/EASCII"), Config::PasswordGenerator_EASCII},
    {QS("generator/AdvancedMode"), Config::PasswordGenerator_AdvancedMode},
    {QS("generator/SpecialChars"), Config::PasswordGenerator_SpecialChars},
    {QS("generator/AdditionalChars"), Config::PasswordGenerator_AdditionalChars},
    {QS("generator/Braces"), Config::PasswordGenerator_Braces},
    {QS("generator/Punctuation"), Config::PasswordGenerator_Punctuation},
    {QS("generator/Quotes"), Config::PasswordGenerator_Quotes},
    {QS("generator/Dashes"), Config::PasswordGenerator_Dashes},
    {QS("generator/Math"), Config::PasswordGenerator_Math},
    {QS("generator/Logograms"), Config::PasswordGenerator_Logograms},
    {QS("generator/ExcludedChars"), Config::PasswordGenerator_ExcludedChars},
    {QS("generator/ExcludeAlike"), Config::PasswordGenerator_ExcludeAlike},
    {QS("generator/EnsureEvery"), Config::PasswordGenerator_EnsureEvery},
    {QS("generator/Length"), Config::PasswordGenerator_Length},
    {QS("generator/WordCount"), Config::PasswordGenerator_WordCount},
    {QS("generator/WordSeparator"), Config::PasswordGenerator_WordSeparator},
    {QS("generator/WordList"), Config::PasswordGenerator_WordList},
    {QS("generator/WordCase"), Config::PasswordGenerator_WordCase},
    {QS("generator/Type"), Config::PasswordGenerator_Type},
    {QS("GUI/HidePasswords"), Config::Deleted},
    {QS("GUI/DarkTrayIcon"), Config::Deleted}};

/**
 * Migrate settings from previous versions.
 */
void Config::migrate()
{
    int previousVersion = m_settings->value("ConfigVersion").toInt();
    if (CONFIG_VERSION <= previousVersion) {
        return;
    }

    // Update renamed keys and drop obsolete keys
    for (const auto& setting : deprecationMap.keys()) {
        QVariant value;
        if (m_settings->contains(setting)) {
            if (setting == QS("IgnoreGroupExpansion") || setting == QS("security/passwordsrepeat") || setting == QS("security/passwordscleartext") || setting == QS("security/passwordemptynodots")) {
                // Keep user's original setting for boolean settings whose meanings were reversed
                value = !m_settings->value(setting).toBool();
            }
            else {
                value = m_settings->value(setting);
            }
            m_settings->remove(setting);
        }
        else if (m_localSettings && m_localSettings->contains(setting)) {
            value = m_localSettings->value(setting);
            m_localSettings->remove(setting);
        }
        else {
            continue;
        }

        if (deprecationMap[setting] == Config::Deleted) {
            continue;
        }

        set(deprecationMap[setting], value);
    }

    m_settings->setValue("ConfigVersion", CONFIG_VERSION);
    sync();
}

Config::Config(QObject* parent)
    : QObject(parent)
{
    // Check if we are running in portable mode, if so store the config files local to the app
    auto portablePath = QCoreApplication::applicationDirPath().append("/%1");
    if (QFile::exists(portablePath.arg(".portable"))) {
        init(portablePath.arg("config/arsenic.ini"), portablePath.arg("config/arsenic_local.ini"));
        return;
    }

    QString configPath;
    QString localConfigPath;

#if defined(Q_OS_WIN)
    configPath      = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    localConfigPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#elif defined(Q_OS_MACOS)
    configPath      = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    localConfigPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
    configPath      = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    localConfigPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#endif

    configPath += "/arsenic";
    localConfigPath += "/arsenic";

#ifdef QT_DEBUG
    configPath += "_debug";
    localConfigPath += "_debug";
#endif

    configPath += ".ini";
    localConfigPath += ".ini";

    init(QDir::toNativeSeparators(configPath), QDir::toNativeSeparators(localConfigPath));
}

Config::~Config()
{
}

void Config::init(const QString& configFileName, const QString& localConfigFileName)
{
    // Upgrade from previous KeePassXC version which stores its config
    // in AppData/Local on Windows instead of AppData/Roaming.
    // Move file to correct location before continuing.
    if (!localConfigFileName.isEmpty() && QFile::exists(localConfigFileName) && !QFile::exists(configFileName)) {
        QDir().mkpath(QFileInfo(configFileName).absolutePath());
        QFile::copy(localConfigFileName, configFileName);
        QFile::remove(localConfigFileName);
        QDir().rmdir(QFileInfo(localConfigFileName).absolutePath());
    }

    m_settings.reset(new QSettings(configFileName, QSettings::IniFormat));
    if (!localConfigFileName.isEmpty() && configFileName != localConfigFileName) {
        m_localSettings.reset(new QSettings(localConfigFileName, QSettings::IniFormat));
    }

    migrate();
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Config::sync);
}

Config* Config::instance()
{
    if (!m_instance) {
        m_instance = new Config(qApp);
    }

    return m_instance;
}

#undef QS
