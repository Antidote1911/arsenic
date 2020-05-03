#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTranslator>

#include "filesystemmodel.h"
#include "skin.h"


/*******************************************************************************

*******************************************************************************/

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

	void session();
    void loadFile(const QString &fileName);

private slots:

	void on_encrypt_clicked();
	void on_decrypt_clicked();
	void on_add_file_clicked();
	void on_add_directory_clicked();
	void on_remove_checked_clicked();
	void on_delete_checked_clicked();
    void setModel(FileSystemModel *ptr_model);
    void on_actionAuto_resize_columns_triggered(bool checked);
    void on_actionLoadList_triggered();
    void on_actionQuit_triggered();
    void on_actionSave_item_list_triggered();
    void on_actionRefresh_view_triggered();
    void on_actionCheck_All_triggered();
    void on_actionUncheck_All_triggered();
    void on_actionAdd_File_triggered();
    void on_actionAdd_Directory_triggered();
    void on_actionRemove_checked_triggered();
    void on_actionDelete_checked_triggered();
    void on_actionConfiguration_triggered();
    void on_checkAll_clicked();
    void on_uncheckAll_clicked();
    void on_viewpass_stateChanged(int arg1);
    void on_generator_clicked();
    void on_actionHash_Calculator_triggered();
    void aboutArsenic();
    void Argon2_tests();
    void dark_theme(bool checked);
    void openFile();
    bool actionSave_triggered();
    bool actionSave_as_triggered();
    void encryptText();
    void decryptText();

private :
    bool maybeSave();
    void setCurrentFile(const QString &fileName);
    QString curFile;
    bool saveFile(const QString &fileName);

protected slots:
    // this slot is called by the language menu actions
    void slotLanguageChanged(QAction* action);
    void setPassword(QString);

protected:
	void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    // this event is called, when a new translator is loaded or the system language is changed
    void changeEvent(QEvent *event) Q_DECL_OVERRIDE;

private:
    const std::unique_ptr<Ui::MainWindow> m_ui;
	void start_crypto(int crypto_type);
    void loadPreferences();
    void savePreferences();
    FileSystemModel *file_model;
    Skin skin;    
    void createLanguageMenu(void); // creates the language menu dynamically from the content of m_langPath
    QString m_langPath; // Path of language files. This is always fixed to /languages.
    void loadLanguage(const QString& rLanguage); // loads a language by the given language shortcur (e.g. de, en)
    QString m_currLang; // contains the currently loaded language
    QTranslator m_translator; // contains the translations for this application
    QTranslator m_translatorQt; // contains the translations for qt
    void switchTranslator(QTranslator& translator, const QString& filename);
};

#endif // MAINWINDOW_H
