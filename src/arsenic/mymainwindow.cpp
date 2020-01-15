#include <iostream>

#include <QMessageBox>
#include <QCloseEvent>
#include <QAction>
#include <QCheckBox>
#include <QDebug>
#include <QListWidgetItem>


#include "quazip.h"
#include "conf.h"
#include "hashcheckdialog.h"
#include "AboutDialog.h"
#include "argontests.h"
#include "PasswordGeneratorWidget.h"
#include "constants.h"

#include "mymainwindow.h"
#include "ui_mymainwindow.h"
#include "myfilesystemmodel.h"
#include "myabstractbar.h"
#include "mysavebar.h"
#include "myloadbar.h"
#include "mysavethread.h"
#include "myloadthread.h"
#include "myencryptbar.h"
#include "mydecryptbar.h"
#include "Config.h"

using namespace  ARs;

typedef std::unique_ptr<MyAbstractBar> MyAbstractBarPtr;

/*******************************************************************************

*******************************************************************************/

MyMainWindow::MyMainWindow(QWidget *parent) :
    QMainWindow(parent), m_ui(new Ui::MyMainWindow), file_model(nullptr)
{
    m_ui->setupUi(this);

    createLanguageMenu();
    loadPreferences();

    setWindowTitle(APP_LONG_NAME);

	setModel(new MyFileSystemModel(nullptr));
    connect(m_ui->actionAbout_Qt, &QAction::triggered, this, &MyMainWindow::aboutQt);
    connect(m_ui->actionAbout_Arsenic, &QAction::triggered, this, &MyMainWindow::aboutArsenic);
    connect(m_ui->actionArgon2_tests, &QAction::triggered, this, &MyMainWindow::Argon2_tests);
    connect(m_ui->actionPassword_Generator, &QAction::triggered, this, &MyMainWindow::on_generator_clicked);
    connect(m_ui->actionDark_Theme,  &QAction::triggered, this, [=]{ dark_theme(); });

	// note that some password information will be discarded if the text characters entered use more
	// than	1 byte per character in UTF-8
    //ui->password_0->setMaxLength(crypto_secretbox_KEYBYTES);
    //ui->password_1->setMaxLength(crypto_secretbox_KEYBYTES);

	// initialize the zipping text codec, specifically, for use in encryption and decryption
	QuaZip::setDefaultFileNameCodec("UTF-8");

    m_ui->encrypt_filename->setMaxLength(ARs::MAX_ENCRYPT_NAME_LENGTH);
    m_ui->actionAuto_resize_columns->setChecked(true);

	// create the temp directory and session.qtlist if they don't exist already
	QDir curr_dir = QDir::current();
    QFile list_file(ARs::DEFAULT_LIST_PATH);

	if(!curr_dir.exists(QString("temp")))
		curr_dir.mkdir(QString("temp"));

	if(!list_file.exists())
	{
		list_file.open(QIODevice::WriteOnly);
		list_file.close();
	}

}

/*******************************************************************************

*******************************************************************************/

MyMainWindow::~MyMainWindow()
{

}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_generator_clicked()
{
        auto pwGenerator = new PasswordGeneratorWidget;
        pwGenerator->setStandaloneMode(true);
        connect(pwGenerator, SIGNAL(appliedPassword(QString)), SLOT(setPassword(QString)));
        connect(pwGenerator, SIGNAL(dialogTerminated()), pwGenerator, SLOT(close()));
        pwGenerator->exec();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::setPassword(QString pass)
{
    m_ui->password_0->setText(pass);
    m_ui->password_1->setText(pass);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::loadPreferences()
{
    if (config()->hasAccessError()) {
            QString warn_text = QString(tr("Access error for config file %1").arg(config()->getFileName()));
            QMessageBox::warning(this, tr("Could not load configuration"), warn_text);
        }

    if (isVisible()) {
        config()->set("GUI/MainWindowGeometry", saveGeometry());
        config()->set("GUI/MainWindowState", saveState());
    }

    loadLanguage(config()->get("GUI/Language").toString());

    restoreGeometry(config()->get("GUI/MainWindowGeometry").toByteArray());
    restoreState(config()->get("GUI/MainWindowState").toByteArray());

    if (config()->get("GUI/darkTheme").toBool())
    {
        m_ui->actionDark_Theme->setChecked(true);
        skin.setSkin("dark");
    }
    else
    {
        m_ui->actionDark_Theme->setChecked(false);
        skin.setSkin("notheme");
    }

    if (config()->get("GUI/showPassword").toBool())
    {
        m_ui->viewpass->setChecked(true);
        m_ui->password_0->setEchoMode(QLineEdit::Normal);
        m_ui->password_1->setEchoMode(QLineEdit::Normal);
    }
    else
    {
        m_ui->viewpass->setChecked(false);
        m_ui->password_0->setEchoMode(QLineEdit::Password);
        m_ui->password_1->setEchoMode(QLineEdit::Password);
    }

}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::savePreferences()
{
    if (isVisible()) {
        config()->set("GUI/MainWindowGeometry", saveGeometry());
        config()->set("GUI/MainWindowState", saveState());
    }
    config()->set("GUI/darkTheme",m_ui->actionDark_Theme->isChecked());
    config()->set("GUI/showPassword", m_ui->viewpass->isChecked());
    config()->set("GUI/Language", m_currLang);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::aboutQt()
{
    QMessageBox::aboutQt(this,tr("About Qt"));
}
void MyMainWindow::aboutArsenic()
{
    auto* aboutDialog = new AboutDialog(this);
    aboutDialog->open();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::session()
{
	// show the main window and load from the default item list
	show();

	MyFileSystemModelPtr clone_model = file_model->cloneByValue();
    MyAbstractBarPtr ptr_mpb(new MyLoadBar(this, clone_model.get(), ARs::DEFAULT_LIST_PATH));

	if(ptr_mpb != nullptr)
	{
		int ret_val = ptr_mpb->exec();

		// if the progress bar successfully loaded the file into the clone model
		if(ret_val == MyAbstractBarPublic::EXEC_SUCCESS)
		{
			// use it to replace the old model
			setModel(clone_model.release());
		}
		else
			setModel(file_model);
	}
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::setModel(MyFileSystemModel *ptr_model)
{
	// when ptr_model is different from the current one, have it replace the old model and resize the
	// columns. otherwise, don't replace but still resize the columns if auto resize was checked
	if(file_model != ptr_model)
	{
		int width_array[MyFileSystemModelPublic::COLUMN_NUM];

        if(!m_ui->actionAuto_resize_columns->isChecked())
		{
			// save the current widths before switching over to the new model, which will reset them
			for(int i = 0; i < MyFileSystemModelPublic::COLUMN_NUM; i++)
                width_array[i] = m_ui->tree_view->columnWidth(i);
		}

		// use it to replace the old model
        m_ui->tree_view->setModel(nullptr);
		delete file_model;
		file_model = ptr_model;
		file_model->setParent(this);

		// setup the view for the new model
        m_ui->tree_view->setModel(file_model);
        m_ui->tree_view->hideColumn(MyFileSystemModelPublic::FULL_PATH_HEADER);
        m_ui->tree_view->hideColumn(MyFileSystemModelPublic::TYPE_HEADER);

        if(!m_ui->actionAuto_resize_columns->isChecked())
		{
			// restore the previous column settings
			for(int i = 0; i < MyFileSystemModelPublic::COLUMN_NUM; i++)
                m_ui->tree_view->setColumnWidth(i, width_array[i]);
		}
	}

	// configure the new width to the contents
    if(m_ui->actionAuto_resize_columns->isChecked())
		for(int i = 0; i < MyFileSystemModelPublic::COLUMN_NUM; i++)
		{
            m_ui->tree_view->resizeColumnToContents(i);
            m_ui->tree_view->setColumnWidth(i, m_ui->tree_view->columnWidth(i) + ARs::EXTRA_COLUMN_SPACE);
        }
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_encrypt_clicked()
{
    start_crypto(ARs::START_ENCRYPT);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_decrypt_clicked()
{
    start_crypto(ARs::START_DECRYPT);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::start_crypto(int crypto_type)
{
	// check if the password fields match
    if(m_ui->password_0->text() == m_ui->password_1->text())
	{
        QString pass = m_ui->password_0->text();
		QString encrypt_name;

		bool checked = false;

		// make sure that there is actually something selected in the model for cryptography
		for(int n = file_model->rowCount(), i = 0; i < n; i++)
		{
			if(file_model->data(file_model->index(i, MyFileSystemModelPublic::CHECKED_HEADER),
				Qt::CheckStateRole) == Qt::Checked)
			{
				checked = true;
				break;
			}
		}

		// check that the password is long enough
        if(pass.toUtf8().size() < ARs::MIN_PASS_LENGTH)
		{
            QString num_text = QString::number(ARs::MIN_PASS_LENGTH);
            QString warn_text = QString(tr("The password entered was too short. ")) + "Please enter a longer "
			"one (at least " + num_text + " ASCII characters/bytes) before continuing.";

			QMessageBox::warning(this, "Password too short!", warn_text);
		}

		// give the user a warning message if nothing was selected
		else if(!checked)
		{
            QString crypto_text = (crypto_type == ARs::START_ENCRYPT) ? "encryption!" : "decryption!";
            QString warn_text = QString(tr("There were no items selected for ") + crypto_text);

            QMessageBox::warning(this, tr("Nothing selected!"), warn_text);
		}

		else
		{            
			// start the encryption process
            bool delete_success = m_ui->delete_success->isChecked();

			MyFileSystemModelPtr clone_model(file_model->cloneByValue());

			MyAbstractBarPtr ptr_mpb;

            if(crypto_type == ARs::START_ENCRYPT)
			{
                // check if a username is set for additionnal data encryption
                if(config()->get("userName").toString()=="")
                {
                    QMessageBox::warning(this, tr("No user name"), tr("You must set a user name or e-mail in configuration !"));
                    return;
                }
				// check if the user wants to rename the encrypted files to something else
                if(m_ui->encrypt_rename->isChecked())
				{
                    encrypt_name = m_ui->encrypt_filename->text();
                    ptr_mpb = MyAbstractBarPtr(new MyEncryptBar(this, clone_model.get(), pass,config()->get("userName").toString(),config()->get("extension").toString(),config()->get("CRYPTO/cryptoAlgo").toString() ,config()->get("CRYPTO/argonMemory").toInt(),config()->get("CRYPTO/argonItr").toInt(),delete_success,
						&encrypt_name));
				}
				else
				{
                    ptr_mpb = MyAbstractBarPtr(new MyEncryptBar(this, clone_model.get(), pass,config()->get("userName").toString(),config()->get("extension").toString(),config()->get("CRYPTO/cryptoAlgo").toString(),config()->get("CRYPTO/argonMemory").toInt(),config()->get("CRYPTO/argonItr").toInt(),delete_success));
				}
			}
			else
                ptr_mpb = MyAbstractBarPtr(new MyDecryptBar(this, clone_model.get(), pass,delete_success));

			if(ptr_mpb != nullptr)
				ptr_mpb->exec();

			// after crypto, replace the old model with the new one
			setModel(clone_model.release());

            // clear the password fields when done
            m_ui->password_0->clear();
            m_ui->password_1->clear();
		}
	}
	else
        QMessageBox::warning(this, tr("Passwords do not match!"), tr("The password fields do not match! "
            "Please make sure they were entered correctly and try again."));
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_add_file_clicked()
{
    QList<QString> file_list = QFileDialog::getOpenFileNames(this, tr("Open File(s)"), QString(),
		QString(), nullptr, QFileDialog::DontResolveSymlinks);
	QList<QString> red_list = QList<QString>();

	bool added = false;

	for(QList<QString>::iterator it = file_list.begin(); it != file_list.end(); ++it)
	{
		int row_num = file_model->rowCount();

		// check to see if the file is already in the model
		if(file_model->isRedundant(*it))
		{
			// create a list of all these files that were redundant
			red_list.append(*it);
		}
		else
		{
			file_model->insertItem(row_num, *it);
			added = true;
		}
	}

	// resize the columns if a file was added
	if(added)
		setModel(file_model);

	if(red_list.size() > 0)
	{
		// create message
        QString warn_title = tr("File(s) already added!");
        QString warn_text = tr("The following file(s) were already added. They will not be added again.");
		QString warn_detail;

		for(int i = 0; i < red_list.size(); i++)
		{
			warn_detail += red_list[i];

			if(i != red_list.size() - 1)
				warn_detail += '\n';
		}

		// display in message box
		QMessageBox msg_box(QMessageBox::Warning, warn_title, warn_text, QMessageBox::Close, this);
		msg_box.setDetailedText(warn_detail);
		msg_box.exec();
	}
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_add_directory_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), QString(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if(dir != QString())
	{
		QString warn_title;
		QString warn_text;
		QString warn_detail;

        // don't let the Windows user add a root directory
        if( QDir(QDir(dir).absolutePath()).isRoot() )
        {
            warn_title = tr("Can't add a root directory!");
            warn_text = tr("The root directory (") + dir + tr(") cannot be added.");

            QMessageBox msg_box(QMessageBox::Warning, warn_title, warn_text, QMessageBox::Close, this);
            msg_box.exec();
            return;
        }

        // don't let the user add an empty directory
        if(QDir(dir).entryList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0)
        {
            warn_title = tr("Can't add empty directory!");
            warn_text = tr("The directory (") + dir + tr(") cannot be added.");

            QMessageBox msg_box(QMessageBox::Warning, warn_title, warn_text, QMessageBox::Close, this);
            msg_box.exec();
            return;
        }

		// is the directory contained by anything already inside the model or a duplicate?
        if(file_model->isRedundant(dir))
		{
			// create message
            warn_title = tr("Directory already added!");
            warn_text = tr("The following directory was already added. It will not be added again.");
			warn_detail = dir;

			// display in message box
			QMessageBox msg_box(QMessageBox::Warning, warn_title, warn_text, QMessageBox::Close, this);
			msg_box.setDetailedText(warn_detail);
			msg_box.exec();
		}

        else
		{
			std::vector<QString> red_list = file_model->makesRedundant(dir);

			// check if the directory chosen makes something redundant inside the model
			if(red_list.size() > 0)
			{
				// ask if they want to add the directory. if so, the redundant items will be removed

                warn_title = tr("Directory makes item(s) redundant!");
                warn_text = tr("The directory chosen (") + dir + tr(") contains item(s) inside it that "
				"were added previously. If you add this new directory, the following redundant items will "
                "be removed.");

                for(unsigned int i = 0; i < red_list.size(); i++)
				{
					warn_detail += red_list[i];

					if(i != red_list.size() - 1)
						warn_detail += '\n';
				}

				// display the message with Ok and Cancel buttons
				QMessageBox msg_box(QMessageBox::Warning, warn_title, warn_text, QMessageBox::Ok |
					QMessageBox::Cancel, this);
				msg_box.setDetailedText(warn_detail);

				if(msg_box.exec() == QMessageBox::Ok)
				{
					file_model->removeItems(red_list);

					file_model->insertItem(0, dir);
					file_model->startDirThread(0);

					// resize the columns
					setModel(file_model);
				}
			}

			// the chosen directory doesn't make anything inside the model redundant, simply add it
			else
			{
				file_model->insertItem(0, dir);
				file_model->startDirThread(0);
				setModel(file_model);
			}
		}

	}	// if(dir != QString())
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_remove_checked_clicked()
{
	// go through the entire model and remove any checked items
	file_model->removeCheckedItems();
	setModel(file_model);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::closeEvent(QCloseEvent *event)
{
	// save the current list before quitting
	MyFileSystemModelPtr clone_model = file_model->cloneByValue();
    MyAbstractBarPtr ptr_mpb(new MySaveBar(this, clone_model.get(), ARs::DEFAULT_LIST_PATH));

    auto ret_val=0;

	if(ptr_mpb != nullptr)
		ret_val = ptr_mpb->exec();

	// if there was a problem saving the list, ask the user before exiting
	if(ret_val == MyAbstractBarPublic::EXEC_FAILURE)
	{
        ret_val = QMessageBox::warning(this, "Error saving session!", "Arsenic wasn't able to save "
			"your current file and directory list. Do you still want to quit?", QMessageBox::Yes |
			QMessageBox::Cancel, QMessageBox::Cancel);

		if(ret_val == QMessageBox::Yes)
			event->accept();
		else
			event->ignore();
	}

	// successfully saved the list, quit
	else
    event->accept();
    savePreferences();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_delete_checked_clicked()
{
	std::vector<MyFileInfoPtr> item_list;

	// get a list of all the items checked for deletion
	for(int n = file_model->rowCount(), i = 0; i < n; i++)
	{
		if(file_model->data(file_model->index(i, MyFileSystemModelPublic::CHECKED_HEADER),
			Qt::CheckStateRole) == Qt::Checked)
		{
			QString full_path = file_model->data(file_model->index(i,
				MyFileSystemModelPublic::FULL_PATH_HEADER)).toString();
			item_list.push_back(MyFileInfoPtr(new MyFileInfo(nullptr, full_path)));
		}
	}

	int total_items = item_list.size();

	if(total_items != 0)
	{
        QMessageBox::StandardButton ret_val = QMessageBox::question(this, tr("Delete items?"), tr("Are you "
            "sure you wish to permanently delete the selected items?"), QMessageBox::Ok |
			QMessageBox::Cancel, QMessageBox::Cancel);

		if(ret_val == QMessageBox::Ok)
		{
			std::vector<QString> result_list;

			for(int i = 0; i < total_items; i++)
			{
				QString full_path = item_list[i]->getFullPath();
				QString item_type = item_list[i]->getType();

				if(item_type == MyFileInfo::typeToString(MyFileInfoPublic::MFIT_FILE))
				{
					if(QFile::remove(full_path))
					{
						file_model->removeItem(full_path);
                        result_list.push_back(tr("Successfully deleted file!"));
					}
					else
                        result_list.push_back(tr("Error deleting file!"));
				}
				else if(item_type == MyFileInfo::typeToString(MyFileInfoPublic::MFIT_DIR))
				{
					if(QDir(full_path).removeRecursively())
					{
						file_model->removeItem(full_path);
                        result_list.push_back(tr("Successfully deleted directory!"));
					}
					else
                        result_list.push_back(tr("Error deleting directory!"));
				}
				else
				{
					file_model->removeItem(full_path);
                    result_list.push_back(tr("Item was not found!"));
				}
			}

			// items were most likely removed from the model, resize columns
			setModel(file_model);

			QString det_string;

			// create a string containing the list of items and results
            for(unsigned int i = 0; i < item_list.size(); i++)
			{
				det_string += item_list[i]->getFullPath();
				det_string += "\n" + result_list[i];

				if(i < item_list.size() - 1)
					det_string += "\n\n";
			}

			QMessageBox del_msg(QMessageBox::Information, "Deletion completed!", "The deletion process "
				"was completed. Click below to show details.", QMessageBox::Close, this);

			del_msg.setDetailedText(det_string);
			del_msg.exec();
		}

		// total_items != 0
	}
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionAuto_resize_columns_triggered(bool checked)
{
    on_actionRefresh_view_triggered();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionLoadList_triggered()
{
    QString list_path = QFileDialog::getOpenFileName(this, "Open List", QDir::currentPath(),
        "Arsenic list files (*.arslist)", nullptr, QFileDialog::DontResolveSymlinks);

    // check if the user actually selected a file
    if(list_path != QString())
    {
        MyFileSystemModelPtr clone_model = file_model->cloneByValue();
        MyAbstractBarPtr ptr_mpb(new MyLoadBar(this, clone_model.get(), list_path));

        if(ptr_mpb != nullptr)
        {
            int ret_val = ptr_mpb->exec();

            // if the progress bar successfully loaded the file into the clone model
            if(ret_val == MyAbstractBarPublic::EXEC_SUCCESS)
            {
                // use it to replace the old model
                setModel(clone_model.release());
            }
        }
    }	// list_path != QString()
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionQuit_triggered()
{
    close();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionSave_item_list_triggered()
{
    QString list_path = QFileDialog::getSaveFileName(this, "Save List", QDir::currentPath(),
        "Arsenic list files (*.arslist)", nullptr, QFileDialog::DontResolveSymlinks);

    // check if the user actually selected a file
    if(list_path != QString())
    {
        MyFileSystemModelPtr clone_model = file_model->cloneByValue();
        MyAbstractBarPtr ptr_mpb(new MySaveBar(this, clone_model.get(), list_path));

        if(ptr_mpb != nullptr)
            ptr_mpb->exec();
    }
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionRefresh_view_triggered()
{
    // creating a new model will recreate and update all the internal MyFileInfo objects
    MyFileSystemModelPtr new_model = file_model->cloneByValue();

    new_model->removeEmpty();
    new_model->removeRootDir();
    new_model->removeRedundant();
    new_model->sorting();

    setModel(new_model.release());
    file_model->startAllDirThread();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionCheck_All_triggered()
{
    int n = file_model->rowCount();

    for(int i = 0; i < n; i++)
        file_model->setData(file_model->index(i), Qt::Checked, Qt::CheckStateRole);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionUncheck_All_triggered()
{
    int n = file_model->rowCount();
    for(int i = 0; i < n; i++)
        file_model->setData(file_model->index(i), Qt::Unchecked, Qt::CheckStateRole);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionAdd_File_triggered()
{
    on_add_file_clicked();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionAdd_Directory_triggered()
{
    on_add_directory_clicked();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionRemove_checked_triggered()
{
    on_remove_checked_clicked();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionDelete_checked_triggered()
{
    on_delete_checked_clicked();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionConfiguration_triggered()
{
    auto* confDialog = new Conf(this);
    confDialog->exec();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_checkAll_clicked()
{
    on_actionCheck_All_triggered();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_uncheckAll_clicked()
{
    on_actionUncheck_All_triggered();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_viewpass_stateChanged(int state)
{
    if (state == 0)
    {
        m_ui->password_0->setEchoMode(QLineEdit::Password);
        m_ui->password_1->setEchoMode(QLineEdit::Password);
    }
    else
    {
        m_ui->password_0->setEchoMode(QLineEdit::Normal);
        m_ui->password_1->setEchoMode(QLineEdit::Normal);
    }
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::on_actionHash_Calculator_triggered()
{
    auto* hashdlg = new HashCheckDialog(this);
    hashdlg->exec();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::Argon2_tests()
{
    auto* Argon2_tests = new Argontests(this);
    Argon2_tests->open();
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::dark_theme()
{
    if (m_ui->actionDark_Theme->isChecked())
    {
        skin.setSkin("dark");
    }
    else
    {
        skin.setSkin("notheme");
    }
}

/*******************************************************************************

*******************************************************************************/

// we create the menu entries dynamically, dependent on the existing translations.
void MyMainWindow::createLanguageMenu(void)
{
    QActionGroup* langGroup = new QActionGroup(m_ui->menuLanguage);
    langGroup->setExclusive(true);

    connect(langGroup, SIGNAL (triggered(QAction *)), this, SLOT (slotLanguageChanged(QAction *)));

    // format systems language
    QString defaultLocale = QLocale::system().name(); // e.g. "de_DE"
    defaultLocale.truncate(defaultLocale.lastIndexOf('_')); // e.g. "de"

    m_langPath = QApplication::applicationDirPath();
    m_langPath.append("/languages");
    QDir dir(m_langPath);
    QStringList fileNames = dir.entryList(QStringList("arsenic_*.qm"));

    for (int i = 0; i < fileNames.size(); ++i)
    {
        // get locale extracted by filename
        QString locale;
        locale = fileNames[i]; // "TranslationExample_de.qm"
        locale.truncate(locale.lastIndexOf('.')); // "TranslationExample_de"
        locale.remove(0, locale.indexOf('_') + 1); // "de"

        QString lang = QLocale::languageToString(QLocale(locale).language());
        QIcon ico(QString("%1/%2.svg").arg(m_langPath).arg(locale));

        QAction *action = new QAction(ico, lang, this);
        action->setCheckable(true);
        action->setData(locale);

        m_ui->menuLanguage->addAction(action);
        langGroup->addAction(action);

        // set default translators and language checked
        if (config()->get("GUI/Language").toString() == locale)
        {
            action->setChecked(true);
        }
    }
}

/*******************************************************************************

*******************************************************************************/

// Called every time, when a menu entry of the language menu is called
void MyMainWindow::slotLanguageChanged(QAction* action)
{
    if(0 != action)
    {
        // load the language dependant on the action content
        loadLanguage(action->data().toString());
        //setWindowIcon(action->icon());
    }
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::loadLanguage(const QString& rLanguage)
{
    if(m_currLang != rLanguage)
    {
        m_currLang = rLanguage;
        QLocale locale = QLocale(m_currLang);
        QLocale::setDefault(locale);
        QString languageName = QLocale::languageToString(locale.language());
        switchTranslator(m_translator, QString("arsenic_%1.qm").arg(rLanguage));
        switchTranslator(m_translatorQt, QString("qt_%1.qm").arg(rLanguage));
        //ui->statusBar->showMessage(tr("Current Language changed to %1").arg(languageName));
    }
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::switchTranslator(QTranslator& translator, const QString& filename)
{
    // remove the old translator
    qApp->removeTranslator(&translator);

    // load the new translator
    QString path = QApplication::applicationDirPath();
    path.append("/languages/");
    if(translator.load(path + filename)) //Here Path and Filename has to be entered because the system didn't find the QM Files else
    qApp->installTranslator(&translator);
}

/*******************************************************************************

*******************************************************************************/

void MyMainWindow::changeEvent(QEvent* event)
{
    if(0 != event)
    {
        switch(event->type())
        {
            // this event is send if a translator is loaded
        case QEvent::LanguageChange:
        m_ui->retranslateUi(this);
            default:
            break;
        break;

   // this event is send, if the system, language changes
        case QEvent::LocaleChange:
        {
            QString locale = QLocale::system().name();
            locale.truncate(locale.lastIndexOf('_'));
            loadLanguage(locale);
        }
        break;
        }
    }
    QMainWindow::changeEvent(event);
}

/*******************************************************************************

*******************************************************************************/
