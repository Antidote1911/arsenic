/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#include <QString>

#include "PasswordHealth.h"

#include "zxcvbn/zxcvbn.h"



namespace
{
    const static int ZXCVBN_ESTIMATE_THRESHOLD = 256;
} // namespace

PasswordHealth::PasswordHealth(double entropy)
{
    init(entropy);
}

PasswordHealth::PasswordHealth(const QString& pwd)
{
    auto entropy = 0.0;
    entropy += ZxcvbnMatch(pwd.left(ZXCVBN_ESTIMATE_THRESHOLD).toUtf8(), nullptr, nullptr);
    if (pwd.length() > ZXCVBN_ESTIMATE_THRESHOLD) {
        // Add the average entropy per character for any characters above the estimate threshold
        auto average = entropy / ZXCVBN_ESTIMATE_THRESHOLD;
        entropy += average * (pwd.length() - ZXCVBN_ESTIMATE_THRESHOLD);
    }
    init(entropy);

}

void PasswordHealth::init(double entropy)
{
    m_score = m_entropy = entropy;

    switch (quality()) {
    case Quality::Bad:
    case Quality::Poor:
        m_scoreReasons << ("Very weak password");
        m_scoreDetails << ("Password entropy is %1 bits")<<QString::number(m_entropy, 'f', 2);
        break;

    case Quality::Weak:
        m_scoreReasons << ("Weak password");
        m_scoreDetails << ("Password entropy is %1 bits")<<(QString::number(m_entropy, 'f', 2));
        break;

    default:
        // No reason or details for good and excellent passwords
        break;
    }
}

void PasswordHealth::setScore(int score)
{
    m_score = score;
}

void PasswordHealth::adjustScore(int amount)
{
    m_score += amount;
}


PasswordHealth::Quality PasswordHealth::quality() const
{
    if (m_score <= 0) {
        return Quality::Bad;
    } else if (m_score < 40) {
        return Quality::Poor;
    } else if (m_score < 75) {
        return Quality::Weak;
    } else if (m_score < 100) {
        return Quality::Good;
    }
    return Quality::Excellent;
}




