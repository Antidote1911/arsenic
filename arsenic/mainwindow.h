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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QLabel>
#include <QProgressBar>
#include <QStandardItemModel>

#include "crypto/Crypto.h"
#include "crypto/hash_tool.h"
#include "config.h"
#include "about.h"
#include "preferences/preferences.h"


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


signals:
    void stopFile(const QString& fileName);

private slots:
    void on_pushAddHash_clicked();
    void on_pushStartHash_clicked();
    void on_pushAddCrypto_clicked();
    void on_pushStartJob_clicked();
    void on_comboDirection_currentIndexChanged(int index);
    void on_pushRemoveAllCrypto_clicked();
    void on_pushClearLog_clicked();
    void on_actionConfiguration_triggered();
    void on_actionQuitter_triggered();
    void on_actionA_propos_de_Arsenic_triggered();
    void on_actionA_propos_de_Botan_triggered();
    void on_actionA_Propos_de_Argon2_triggered();
    void on_actionA_Propos_de_Qt_triggered();
    void on_pushRemoveAllHash_clicked();
    void on_comboHash_currentIndexChanged(const QString &arg1);

public slots:
    void onPercentProgress(const QString& path, qint64 percent, QString source);
    void onMessageChanged(QString algo, QString message, QString filename);
    void onResultHashChanged(QString algo, QString message, QString filename);

private:
    Ui::MainWindow *ui;
    ARs::Preferences m_prefs;


    QStandardItemModel *fileListModelCrypto;
    QStandardItemModel *fileListModelHash;

    Crypto_Thread *Crypto;
    Hash_Tool *Hash;

    void loadOptions();
    void saveOptions();
    void addFilePathToModel(const QString& filePath,QString cryptoOrHash);
    void removeFile(const QModelIndex& index);
    void saveSettings();
    void restoreGeomAndUi();
    void restoreLogText();
    void cryptoFileView();
    void cryptoHashView();
    void delegate();
    void addFiles(QString cryptoOrHash);
    QStringList getListFiles(QString cryptoOrHash);


protected:
    void closeEvent(QCloseEvent *event);
};

#endif // MAINWINDOW_H
