#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTranslator>
#include <QActionGroup>

#include "fileCrypto.h"
#include "textcrypto.h"
#include "skin.h"
#include "loghtml.h"
#include "consts.h"
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

    void session();

  public slots:
    void onPercentProgress(const QString &path, quint32 percent);
    void onMessageChanged(const QString message);
    void AddEncryptedFile(QString filepath);
    void removeDeletedFile(QString filepath);

  private slots:
    void quit();
    void configuration();
    void viewPassStateChanged(quint32 index);
    void generator();
    void hashCalculator();
    void aboutArsenic();
    void Argon2_tests();
    void dark_theme(bool checked);
    void openTxtFile();
    bool saveTxtFile();
    bool saveTxtFileAs();
    void encryptText();
    void decryptText();
    void addFiles();
    void addFolder();
    void switchTab(quint32 index);
    void clearEditor();
    void encryptFiles();
    void decryptFiles();
    void abortJob();
    void clearListFiles();
    void clearLog();

  protected slots:
    // this slot is called by the language menu actions
    void slotLanguageChanged(QAction *m1_action);
    void setPassword(QString);

  protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

    // this event is called, when a new translator is loaded or the system language is changed
    void changeEvent(QEvent *event) Q_DECL_OVERRIDE;

  private:
    const std::unique_ptr<Ui::MainWindow> m_ui;
    std::unique_ptr<Crypto_Thread> m_file_crypto;
    std::unique_ptr<textCrypto> m_text_crypto;
    std::unique_ptr<logHtml> m_log;
    std::unique_ptr<Skin> m_skin;

    std::unique_ptr<QStandardItemModel> fileListModelCrypto;
    std::unique_ptr<Delegate> m_delegate;
    std::unique_ptr<QActionGroup> m_langGroup;

    const std::unique_ptr<consts> m_const;

    void loadPreferences();
    void savePreferences();
    void createLanguageMenu(void);               // creates the language menu dynamically from the content of m_langPath
    QString m_langPath;                          // Path of language files. This is always fixed to /languages.
    void loadLanguage(const QString &rLanguage); // loads a language by the given language shortcur (e.g. de, en)

    QString m_currLang;         // contains the currently loaded language
    QTranslator m_translator;   // contains the translations for this application
    QTranslator m_translatorQt; // contains the translations for qt
    void switchTranslator(QTranslator &translator, const QString &filename);
    bool maybeSave();
    void setCurrentFile(const QString &fileName);

    QString tempPath;

    QString curFile;
    bool saveFile(const QString &fileName);
    void loadFile(const QString &fileName);

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
    void displayEmptyEditor();
};

#endif // MAINWINDOW_H
