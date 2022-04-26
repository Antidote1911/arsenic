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

#include "passwordGenerator.h"
#include "zxcvbn.h"

#include <QtGlobal>
#include "botan_all.h"

const char *PasswordGenerator::DefaultExcludedChars = "";

PasswordGenerator::PasswordGenerator()
    : m_length(0),
      m_classes(),
      m_flags(),
      m_excluded(PasswordGenerator::DefaultExcludedChars)
{
}

double PasswordGenerator::estimateEntropy(const QString &password)
{
    return (ZxcvbnMatch(password.toLatin1(), nullptr, nullptr));
}

void PasswordGenerator::setLength(int length)
{
    if (length <= 0) {
        m_length = DefaultLength;
        return;
    }

    m_length = length;
}

void PasswordGenerator::setCharClasses(const CharClasses &classes)
{
    if (classes == 0) {
        m_classes = DefaultCharset;
        return;
    }

    m_classes = classes;
}

void PasswordGenerator::setFlags(const GeneratorFlags &flags)
{
    m_flags = flags;
}

void PasswordGenerator::setExcludedChars(const QString &chars)
{
    m_excluded = chars;
}

QString PasswordGenerator::generatePassword() const
{
    Q_ASSERT(isValid());

    const QVector<PasswordGroup> groups = passwordGroups();

    QVector<QChar> passwordChars;
    for (const PasswordGroup &group : groups) {
        for (QChar ch : group) {
            passwordChars.append(ch);
        }
    }

    QString password;

    if (m_flags & CharFromEveryGroup) {
        for (const auto &group : groups) {
            int pos = Botan::Sodium::randombytes_uniform(static_cast<quint32>(group.size()));

            password.append(group[pos]);
        }

        for (int i = groups.size(); i < m_length; i++) {
            int pos = Botan::Sodium::randombytes_uniform(static_cast<quint32>(passwordChars.size()));

            password.append(passwordChars[pos]);
        }

        // shuffle chars
        for (int i = (password.size() - 1); i >= 1; i--) {
            int j = Botan::Sodium::randombytes_uniform(static_cast<quint32>(i + 1));

            QChar tmp   = password[i];
            password[i] = password[j];
            password[j] = tmp;
        }
    }
    else {
        for (int i = 0; i < m_length; i++) {
            int pos = Botan::Sodium::randombytes_uniform(static_cast<quint32>(passwordChars.size()));

            password.append(passwordChars[pos]);
        }
    }

    return (password);
}

bool PasswordGenerator::isValid() const
{
    if (m_classes == 0) {
        return (false);
    }
    else if (m_length == 0) {
        return (false);
    }

    if ((m_flags & CharFromEveryGroup) && (m_length < numCharClasses())) {
        return (false);
    }

    return (!passwordGroups().isEmpty());
}

QVector<PasswordGroup> PasswordGenerator::passwordGroups() const
{
    QVector<PasswordGroup> passwordGroups;

    if (m_classes & LowerLetters) {
        PasswordGroup group;

        for (int i = 97; i <= (97 + 25); i++) {
            if ((m_flags & ExcludeLookAlike) && (i == 108)) { // "l"
                continue;
            }

            group.append( static_cast < char >(i));
        }

        passwordGroups.append(group);
    }

    if (m_classes & UpperLetters) {
        PasswordGroup group;

        for (int i = 65; i <= (65 + 25); i++) {
            if ((m_flags & ExcludeLookAlike) && ((i == 73) || (i == 79))) { // "I" and "O"
                continue;
            }

            group.append( static_cast < char >(i));
        }

        passwordGroups.append(group);
    }

    if (m_classes & Numbers) {
        PasswordGroup group;

        for (int i = 48; i < (48 + 10); i++) {
            if ((m_flags & ExcludeLookAlike) && ((i == 48) || (i == 49))) { // "0" and "1"
                continue;
            }

            group.append( static_cast < char >(i));
        }

        passwordGroups.append(group);
    }

    if (m_classes & Braces) {
        PasswordGroup group;

        // ()[]{}
        group.append(static_cast < char >(40));
        group.append(static_cast < char >(41));
        group.append(static_cast < char >(91));
        group.append(static_cast < char >(93));
        group.append(static_cast < char >(123));
        group.append(static_cast < char >(125));

        passwordGroups.append(group);
    }

    if (m_classes & Punctuation) {
        PasswordGroup group;

        // .,:;
        group.append(static_cast < char >(44));
        group.append(static_cast < char >(46));
        group.append(static_cast < char >(58));
        group.append(static_cast < char >(59));

        passwordGroups.append(group);
    }

    if (m_classes & Quotes) {
        PasswordGroup group;

        // "'
        group.append(static_cast < char >(34));
        group.append(static_cast < char >(39));

        passwordGroups.append(group);
    }

    if (m_classes & Dashes) {
        PasswordGroup group;

        // -/\_|
        group.append(static_cast < char >(45));
        group.append(static_cast < char >(47));
        group.append(static_cast < char >(92));
        group.append(static_cast < char >(95));

        if (!(m_flags & ExcludeLookAlike)) {
            group.append(static_cast < char >(124)); // "|"
        }

        passwordGroups.append(group);
    }

    if (m_classes & Math) {
        PasswordGroup group;

        // !*+-<=>?
        group.append(static_cast < char >(33));
        group.append(static_cast < char >(42));
        group.append(static_cast < char >(43));
        group.append(static_cast < char >(60));
        group.append(static_cast < char >(61));
        group.append(static_cast < char >(62));
        group.append(static_cast < char >(63));

        passwordGroups.append(group);
    }

    if (m_classes & Logograms) {
        PasswordGroup group;

        // #$%&
        for (int i = 35; i <= 38; i++) {
            group.append(static_cast < char >(i));
        }
        // @^`~
        group.append(static_cast < char >(64));
        group.append(static_cast < char >(94));
        group.append(static_cast < char >(96));
        group.append(static_cast < char >(126));

        passwordGroups.append(group);
    }

    if (m_classes & EASCII) {
        PasswordGroup group;

        // [U+0080, U+009F] are C1 control characters,
        // U+00A0 is non-breaking space
        for (int i = 161; i <= 172; i++) {
            group.append(static_cast < char >(i));
        }
        // U+00AD is soft hyphen (format character)
        for (int i = 174; i <= 255; i++) {
            if ((m_flags & ExcludeLookAlike) && (i == 249)) { // "﹒"
                continue;
            }

            group.append(static_cast < char >(i));
        }

        passwordGroups.append(group);
    }

    // Loop over character groups and remove excluded characters from them;
    // remove empty groups
    int i = 0;
    while (i != passwordGroups.size()) {
        PasswordGroup group = passwordGroups[i];

        for (QChar ch : m_excluded) {
            int j = group.indexOf(ch);
            while (j != -1) {
                group.remove(j);
                j = group.indexOf(ch);
            }
        }

        if (!group.isEmpty()) {
            passwordGroups.replace(i, group);
            ++i;
        }
        else {
            passwordGroups.remove(i);
        }
    }

    return (passwordGroups);
}

int PasswordGenerator::numCharClasses() const
{
    int numClasses = 0;

    if (m_classes & LowerLetters) {
        numClasses++;
    }

    if (m_classes & UpperLetters) {
        numClasses++;
    }

    if (m_classes & Numbers) {
        numClasses++;
    }

    if (m_classes & Braces) {
        numClasses++;
    }

    if (m_classes & Punctuation) {
        numClasses++;
    }

    if (m_classes & Quotes) {
        numClasses++;
    }

    if (m_classes & Dashes) {
        numClasses++;
    }

    if (m_classes & Math) {
        numClasses++;
    }

    if (m_classes & Logograms) {
        numClasses++;
    }

    if (m_classes & EASCII) {
        numClasses++;
    }

    return (numClasses);
}
