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

#ifndef HASH_TOOL_H
#define HASH_TOOL_H
#include <QThread>
#include <QString>


class Hash_Tool : public QThread
{
    Q_OBJECT
public:
    explicit Hash_Tool(QObject *parent = 0);
    void run();
    bool aborted;
    void setParam(QStringList filenames, QString algo);


signals:
    void PercentProgressHashChanged(QString, qint64, QString);
    void statusResult(QString, QString, QString);

private:
    QStringList m_filenames;
    QString m_algo;

    void calculateHash(QStringList& filename, QString nameHash);

public slots:


};

#endif // HASH_TOOL_H
