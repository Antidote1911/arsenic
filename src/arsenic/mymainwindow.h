#ifndef MYMAINWINDOW_H
#define MYMAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTranslator>

#include "myfilesystemmodel.h"
#include "preferences.h"
#include "skin.h"


/*******************************************************************************



*******************************************************************************/


namespace Ui {
class MyMainWindow;
}

class MyMainWindow : public QMainWindow
{
	Q_OBJECT

public:
    explicit MyMainWindow(QWidget *parent = nullptr);
    ~MyMainWindow() override;

	void session();

private slots:

	void on_encrypt_clicked();
	void on_decrypt_clicked();
	void on_add_file_clicked();
	void on_add_directory_clicked();
	void on_remove_checked_clicked();
	void on_delete_checked_clicked();
	void setModel(MyFileSystemModel *ptr_model);
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
    void aboutQt();
    void aboutArsenic();
    void Argon2_tests();
    void dark_theme();

protected slots:
  // this slot is called by the language menu actions
  void slotLanguageChanged(QAction* action);

protected:
	void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    // this event is called, when a new translator is loaded or the system language is changed
      void changeEvent(QEvent*);

private:
    ARs::Preferences *m_prefs;
    QSettings *settings;

	void start_crypto(int crypto_type);
    void loadPreferences();
    void savePreferences();
	Ui::MyMainWindow *ui;
	MyFileSystemModel *file_model;
	QString password;
    Skin skin;
    // creates the language menu dynamically from the content of m_langPath
      void createLanguageMenu(void);
      QString m_langPath; // Path of language files. This is always fixed to /languages.
      // loads a language by the given language shortcur (e.g. de, en)
        void loadLanguage(const QString& rLanguage);
        QString m_currLang; // contains the currently loaded language
        QTranslator m_translator; // contains the translations for this application
          QTranslator m_translatorQt; // contains the translations for qt
          void switchTranslator(QTranslator& translator, const QString& filename);
};

#endif // MYMAINWINDOW_H
