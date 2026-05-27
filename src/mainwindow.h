#pragma once

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTranslator>
#include <QActionGroup>

#include "arscore/CryptoThread.h"
#include "arscore/consts.h"
#include "Delegate.h"
#include <memory>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

  public slots:
    void onPercentProgress(const QString &path, quint32 percent);
    void onMessageChanged(const QString message);
    void AddEncryptedFile(QString filepath);
    void removeDeletedFile(QString filepath);
    void restartApp();

  private slots:
    void quit();
    void configuration();
    void viewPassStateChanged(quint32 index);
    void generator();
    void hashCalculator();
    void aboutArsenic();
    void addFiles();
    void addFolder();
    void switchTab(quint32 index);
    void encryptFiles();
    void decryptFiles();
    void changePassword();
    void abortJob();
    void clearListFiles();
    void clearLog();
    void reboot();

  protected slots:
    void setPassword(QString);

  protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    bool eventFilter(QObject *obj, QEvent *event) override;

  private:
    const std::unique_ptr<Ui::MainWindow> m_ui;
    std::unique_ptr<Crypto_Thread> m_file_crypto;

    std::unique_ptr<QStandardItemModel> fileListModelCrypto;
    std::unique_ptr<Delegate> m_delegate;
    std::unique_ptr<QActionGroup> m_langGroup;

    const std::unique_ptr<consts> m_const;

    void applyTheme();
    void initViewMenu();
    void loadPreferences();
    void savePreferences(); // creates the language menu dynamically from the content of m_langPath

    void cryptoFileView();
    void delegate();
    void removeFile(const QModelIndex &index);
    void addFilePathToModel(const QString &filePath);

    QStringList getListFiles();
    void loadLogFile();

    // simples messages box display
    void displayMessageBox(const QString &title, const QString &text);
    void displayPasswordNotMatch();
    void displayEmptyPassword();
    void displayEmptyJob();
};
