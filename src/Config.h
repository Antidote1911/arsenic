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

#pragma once

#include <QPointer>
#include <memory>
#include <QVariant>

class QSettings;

class Config : public QObject {
    Q_OBJECT

  public:
    Q_DISABLE_COPY(Config)

    enum ConfigKey {
        GUI_showPassword,
        GUI_deleteFinished,
        GUI_Language,
        GUI_showToolbar,
        GUI_lastDirectory,
        GUI_currentIndexTab,
        GUI_AddEncrypted,
        GUI_ApplicationTheme,

        GUI_MainWindowGeometry,
        GUI_MainWindowState,

        CRYPTO_argonMemory,
        CRYPTO_argonItr,
        CRYPTO_algorithm,

        SECURITY_clearclipboard,
        SECURITY_clearclipboardtimeout,

        PasswordGenerator_LowerCase,
        PasswordGenerator_UpperCase,
        PasswordGenerator_Numbers,
        PasswordGenerator_EASCII,
        PasswordGenerator_AdvancedMode,
        PasswordGenerator_SpecialChars,
        PasswordGenerator_AdditionalChars,
        PasswordGenerator_Braces,
        PasswordGenerator_Punctuation,
        PasswordGenerator_Quotes,
        PasswordGenerator_Dashes,
        PasswordGenerator_Math,
        PasswordGenerator_Logograms,
        PasswordGenerator_ExcludedChars,
        PasswordGenerator_ExcludeAlike,
        PasswordGenerator_EnsureEvery,
        PasswordGenerator_Length,
        PasswordGenerator_WordCount,
        PasswordGenerator_WordSeparator,
        PasswordGenerator_WordList,
        PasswordGenerator_WordCase,
        PasswordGenerator_Type,
        PasswordGenerator_ViewPassword,

        // Special internal value
        Deleted
    };

    ~Config() override;
    QVariant get(ConfigKey key);
    QString getFileName();
    void set(ConfigKey key, const QVariant& value);
    void remove(ConfigKey key);
    bool hasAccessError();
    void sync();
    void resetToDefaults();

    static Config* instance();

  signals:
    void changed(ConfigKey key);

  private:
    explicit Config(QObject* parent);
    void init(const QString& configFileName, const QString& localConfigFileName = "");

    static QPointer<Config> m_instance;

    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<QSettings> m_localSettings;
    QHash<QString, QVariant> m_defaults;
};

inline Config* config()
{
    return Config::instance();
}
