#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>

#include "Config.h"
#include "Delegate.h"
#include "aboutDialog.h"
#include "argonTests.h"
#include "configDialog.h"
#include "constants.h"
#include "crypto.h"
#include "divers.h"
#include "hashcheckdialog.h"
#include "passwordGeneratorDialog.h"

using namespace ARs;

/*******************************************************************************

*******************************************************************************/

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);
    Crypto = new Crypto_Thread(this);

    createLanguageMenu();
    loadPreferences();
    loadLogFile();

    setWindowTitle(APP_LONG_NAME);
    cryptoFileView();
    delegate();

    // General
    connect(m_ui->menuAboutArsenic, &QAction::triggered, this, [=] { aboutArsenic(); });
    connect(m_ui->menuHashCalculator, &QAction::triggered, this, [=] { hashCalculator(); });
    connect(m_ui->menuAboutQt, &QAction::triggered, this, [=] { qApp->aboutQt(); });
    connect(m_ui->menuPassGenerator, &QAction::triggered, this, [=] { generator(); });
    connect(m_ui->pushPassGenerator, &QPushButton::clicked, this, [=] { generator(); });
    connect(m_ui->menuQuit, &QAction::triggered, this, [=] { quit(); });
    connect(m_ui->menuQuit2, &QAction::triggered, this, [=] { quit(); });
    connect(m_ui->menuQuit3, &QAction::triggered, this, [=] { quit(); });
    connect(m_ui->menuConfiguration, &QAction::triggered, this, [=] { configuration(); });
    connect(m_ui->menuDarkTheme, &QAction::triggered, this, [=](const bool& checked) { dark_theme(checked); });
    connect(m_ui->menuViewToolbar, &QAction::triggered, this, [=](const bool& checked) { m_ui->toolBar->setVisible(checked); });
    connect(m_ui->toolBar, &QToolBar::visibilityChanged, this, [=](const bool& checked) { m_ui->menuViewToolbar->setChecked(checked); });
    connect(m_ui->menuArgon2Tests, &QAction::triggered, this, [=] { Argon2_tests(); });
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, [=](const quint32& index) { switchTab(index); });
    connect(m_ui->comboViewpass, &QCheckBox::stateChanged, this, [=](const quint32& index) { viewPassStateChanged(index); });

    // EncryptPad
    connect(m_ui->menuOpenTxt, &QAction::triggered, this, [=] { openTxtFile(); });
    connect(m_ui->menuSaveTxt, &QAction::triggered, this, [=] { saveTxtFile(); });
    connect(m_ui->menuSaveTxtAs, &QAction::triggered, this, [=] { saveTxtFileAs(); });
    connect(m_ui->menuEncryptTxt, &QAction::triggered, this, [=] { encryptText(); });
    connect(m_ui->menuDecryptTxt, &QAction::triggered, this, [=] { decryptText(); });
    connect(m_ui->menuClearEditor, &QAction::triggered, this, [=] { clearEditor(); });
    connect(m_ui->pushEncryptTxt, &QPushButton::clicked, this, [=] { encryptText(); });
    connect(m_ui->pushDecryptTxt, &QPushButton::clicked, this, [=] { decryptText(); });

    // EncryptFile
    connect(m_ui->menuAddFiles, &QAction::triggered, this, [=] { addFiles(); });
    connect(m_ui->menuRemoveAllFiles, &QAction::triggered, this, [=] { clearListFiles(); });
    connect(m_ui->menuEncryptList, &QAction::triggered, this, [=] { encryptFiles(); });
    connect(m_ui->menuDecryptList, &QAction::triggered, this, [=] { decryptFiles(); });
    connect(m_ui->pushEncrypt, &QPushButton::clicked, this, [=] { encryptFiles(); });
    connect(m_ui->pushDecrypt, &QPushButton::clicked, this, [=] { decryptFiles(); });
    connect(m_ui->menuAbortJob, &QAction::triggered, this, [=] { abortJob(); });
    connect(Crypto, SIGNAL(updateProgress(QString, quint32)), this, SLOT(onPercentProgress(QString, quint32)));
    connect(Crypto, SIGNAL(statusMessage(QString)), this, SLOT(onMessageChanged(QString)));
    connect(Crypto, SIGNAL(addEncrypted(QString)), this, SLOT(AddEncryptedFile(QString)));
    connect(Crypto, SIGNAL(addDecrypted(QString)), this, SLOT(AddDecryptedFile(QString)));
    connect(Crypto, SIGNAL(sourceDeletedAfterSuccess(QString)), this, SLOT(removeDeletedFile(QString)));
    // connect(this,&MainWindow::on_stop,&myjob,&MyJob::obj_slot_stop);

    // note that some password information will be discarded if the text
    // characters entered use more than	1 byte per character in UTF-8
    // ui->password_0->setMaxLength(crypto_secretbox_KEYBYTES);
    // ui->password_1->setMaxLength(crypto_secretbox_KEYBYTES);

    // create the temp directory and session.qtlist if they don't exist already

    QFile list_file(DEFAULT_LIST_PATH);
    if (!list_file.exists()) {
        list_file.open(QIODevice::WriteOnly);
        list_file.close();
    }
}

MainWindow::~MainWindow() { }

void MainWindow::session()
{
    // show the main window and load from the default item list
    show();
}

void MainWindow::removeDeletedFile(QString filepath)
{
    QList<QStandardItem*> items;
    QStandardItem* item;
    items = fileListModelCrypto->findItems(filepath, Qt::MatchExactly, 2);

    item = items[0];
    auto index = item->row();

    if (fileListModelCrypto->hasChildren()) {
        fileListModelCrypto->removeRow(index);
    }
}

void MainWindow::AddEncryptedFile(QString filepath)
{
    if (config()->get("GUI/AddEncrypted").toBool()) {
        addFilePathToModel(filepath);
    }
}

void MainWindow::AddDecryptedFile(QString filepath)
{
    if (config()->get("GUI/AddDecrypted").toBool()) {
        addFilePathToModel(filepath);
    }
}

void MainWindow::loadLogFile()
{
    QFile logfile("arsenic.log.html");
    logfile.open(QIODevice::ReadOnly | QIODevice::Text);
    m_ui->textLogs->setText(logfile.readAll());
}

void MainWindow::abortJob() { Crypto->aborted = true; }

void MainWindow::onMessageChanged(QString message)
{
    QTextCursor c = m_ui->textLogs->textCursor();
    c.movePosition(QTextCursor::End);
    m_ui->textLogs->setTextCursor(c);
    m_ui->textLogs->append(message);

    QFile logfile("arsenic.log.html");
    logfile.open(QIODevice::ReadWrite | QIODevice::Text);
    QTextStream out(&logfile);
    out << m_ui->textLogs->toHtml() << endl;
    logfile.close();
}

void MainWindow::encryptFiles()
{
    if (getListFiles().isEmpty()) {
        displayEmptyJob();
        return;
    }

    if (m_ui->password_0->text().isEmpty()) {
        displayEmptyPassword();
        return;
    }

    if (m_ui->password_0->text() != m_ui->password_1->text()) {
        displayPasswordNotMatch();
        return;
    }

    Crypto->setParam(true, getListFiles(), m_ui->password_0->text(),
        config()->get("CRYPTO/argonMemory").toInt(),
        config()->get("CRYPTO/argonItr").toInt(),
        m_ui->CheckDeleteFiles->isChecked());

    Crypto->start();
}

void MainWindow::decryptFiles()
{
    if (getListFiles().isEmpty()) {
        displayEmptyJob();
        return;
    }

    if (m_ui->password_0->text().isEmpty()) {
        displayEmptyPassword();
        return;
    }

    Crypto->setParam(false, getListFiles(), m_ui->password_0->text(),
        config()->get("CRYPTO/argonMemory").toInt(),
        config()->get("CRYPTO/argonItr").toInt(),
        m_ui->CheckDeleteFiles->isChecked());

    Crypto->start();
}

QStringList MainWindow::getListFiles()
{
    const auto rowCountCrypto = fileListModelCrypto->rowCount();
    QStringList fileList;

    if (0 < rowCountCrypto) {
        for (auto row = 0; row < rowCountCrypto; ++row) {
            QStandardItem* item = fileListModelCrypto->item(row, 2);
            fileList.append(item->text());
        }
    }

    return fileList;
}

void MainWindow::clearEditor() { m_ui->cryptoPadEditor->clear(); }

void MainWindow::onPercentProgress(const QString& path, quint32 percent)
{
    QList<QStandardItem*> items;
    QStandardItem* item;
    QStandardItem* progressItem;

    items = fileListModelCrypto->findItems(path, Qt::MatchExactly, 2);

    if (0 < items.size()) {
        item = items[0];
        auto index = item->row();

        if (nullptr != item) {
            progressItem = fileListModelCrypto->item(index, 4);

            if (nullptr != progressItem) {
                progressItem->setData(percent, Qt::DisplayRole);
            }
        }
    }
}

void MainWindow::switchTab(quint32 index)
{
    if (index == 0) {
        m_ui->menuCryptopad->menuAction()->setVisible(false);
        m_ui->menuLog->menuAction()->setVisible(false);
        m_ui->menuFile->menuAction()->setVisible(true);
        m_ui->password_0->show();
        m_ui->password_1->show();
        m_ui->comboViewpass->show();
        m_ui->password_label->show();
        m_ui->label->show();
        m_ui->pushPassGenerator->show();
    }
    if (index == 1) {
        m_ui->menuCryptopad->menuAction()->setVisible(true);
        m_ui->menuLog->menuAction()->setVisible(false);
        m_ui->menuFile->menuAction()->setVisible(false);
        m_ui->password_0->show();
        m_ui->password_1->show();
        m_ui->comboViewpass->show();
        m_ui->password_label->show();
        m_ui->label->show();
        m_ui->pushPassGenerator->show();
    }
    if (index == 2) {
        m_ui->menuLog->menuAction()->setVisible(true);
        m_ui->menuCryptopad->menuAction()->setVisible(false);
        m_ui->menuFile->menuAction()->setVisible(false);
        m_ui->password_0->hide();
        m_ui->password_1->hide();
        m_ui->comboViewpass->hide();
        m_ui->password_label->hide();
        m_ui->label->hide();
        m_ui->pushPassGenerator->hide();
    }
}

void MainWindow::cryptoFileView()
{
    fileListModelCrypto = new QStandardItemModel(0, 5);
    fileListModelCrypto->setHeaderData(0, Qt::Horizontal, tr("Remove File"));
    fileListModelCrypto->setHeaderData(1, Qt::Horizontal, tr("Name"));
    fileListModelCrypto->setHeaderData(2, Qt::Horizontal, tr("Path"));
    fileListModelCrypto->setHeaderData(3, Qt::Horizontal, tr("Size"));
    fileListModelCrypto->setHeaderData(4, Qt::Horizontal, tr("Progress"));

    QHeaderView* headerCrypto = m_ui->fileListViewCrypto->horizontalHeader();
    headerCrypto->setStretchLastSection(true);

    // hide index numbers
    m_ui->fileListViewCrypto->verticalHeader()->setVisible(false);

    // Minimize all row height in Tables
    m_ui->fileListViewCrypto->verticalHeader()->setDefaultSectionSize(
        m_ui->fileListViewCrypto->verticalHeader()->minimumSectionSize());
}

void MainWindow::delegate()
{
    // Custom delegate paints progress bar and file close button for each file
    Delegate* delegate = new Delegate { this };

    m_ui->fileListViewCrypto->setItemDelegate(delegate);
    m_ui->fileListViewCrypto->setModel(fileListModelCrypto);

    connect(delegate, &Delegate::removeRow, this, &MainWindow::removeFile);
}

void MainWindow::addFiles()
{
    // Open a file dialog to get files
    const auto files = QFileDialog::getOpenFileNames(
        this, tr("Add File(s)"),
        config()->get("GUI/lastDirectory").toByteArray());

    if (files.isEmpty()) // if no file selected
        return;

    // Save this directory to return to later
    const auto fileName { files[0] };
    config()->set("GUI/lastDirectory", fileName.left(fileName.lastIndexOf("/")));

    for (const QString& file : files) // add files to the model
    {
        addFilePathToModel(file);
    }
}

void MainWindow::addFilePathToModel(const QString& filePath)
{
    QFileInfo fileInfo { filePath };

    if (fileInfo.exists() && fileInfo.isFile()) // If the file exists, add it to the model
    {
        const auto fileSize { getFileSize((fileInfo.size())) };
        const auto fileName { fileInfo.fileName() };

        const auto fileItem { new QStandardItem { fileName } };
        fileItem->setDragEnabled(false);
        fileItem->setDropEnabled(false);
        fileItem->setEditable(false);
        fileItem->setSelectable(false);
        fileItem->setToolTip(fileName);
        const auto fileVariant { QVariant::fromValue(fileName) };
        fileItem->setData(fileVariant);

        const auto pathItem { new QStandardItem { filePath } };
        pathItem->setDragEnabled(false);
        pathItem->setDropEnabled(false);
        pathItem->setEditable(false);
        pathItem->setSelectable(false);
        pathItem->setToolTip(filePath);
        const auto pathVariant { QVariant::fromValue(filePath) };
        pathItem->setData(pathVariant);

        const auto sizeItem { new QStandardItem { fileSize } };
        sizeItem->setDragEnabled(false);
        sizeItem->setDropEnabled(false);
        sizeItem->setEditable(false);
        sizeItem->setSelectable(false);
        sizeItem->setToolTip(QString::number(fileInfo.size()) + " bytes");
        const auto sizeVariant { QVariant::fromValue(fileSize) };
        sizeItem->setData(sizeVariant);

        const auto progressItem { new QStandardItem {} };
        progressItem->setDragEnabled(false);
        progressItem->setDropEnabled(false);
        progressItem->setEditable(false);
        progressItem->setSelectable(false);

        const auto closeFileItem { new QStandardItem {} };
        closeFileItem->setDragEnabled(false);
        closeFileItem->setDropEnabled(false);
        closeFileItem->setEditable(false);
        closeFileItem->setSelectable(false);

        const QList<QStandardItem*> items = { closeFileItem, fileItem, pathItem,
            sizeItem, progressItem };

        // Search to see if this item is already in the model
        auto addNewItem = true;

        const auto rowCryptoCount = fileListModelCrypto->rowCount();

        for (auto row = 0; row < rowCryptoCount; ++row) {
            auto testItem = fileListModelCrypto->item(row, 1);
            if (testItem->data().toString() == pathItem->data().toString()) {
                addNewItem = false;
            }
        }

        if (addNewItem) { // Add the item to the model if it's new
            fileListModelCrypto->appendRow(items);
        }
    }

    m_ui->fileListViewCrypto->resizeColumnsToContents();
    m_ui->fileListViewCrypto->setColumnWidth(2, 100); // Limit the "path" to 100
    // px

    // End if file exists and is a file
}

void MainWindow::removeFile(const QModelIndex& index)
{
    fileListModelCrypto->removeRow(index.row());
}

void MainWindow::clearListFiles()
{
    if (fileListModelCrypto->hasChildren()) {
        fileListModelCrypto->removeRows(0, fileListModelCrypto->rowCount());
    }
}

void MainWindow::generator()
{
    auto pwGenerator { new PasswordGeneratorDialog };
    pwGenerator->setStandaloneMode(true);
    connect(pwGenerator, SIGNAL(appliedPassword(QString)), SLOT(setPassword(QString)));
    connect(pwGenerator, SIGNAL(dialogTerminated()), pwGenerator, SLOT(close()));
    pwGenerator->exec();
}

void MainWindow::setPassword(QString pass)
{
    m_ui->password_0->setText(pass);
    m_ui->password_1->setText(pass);
}

void MainWindow::loadPreferences()
{
    if (config()->hasAccessError()) {
        auto warn_text = QString(tr("Access error for config file %1").arg(config()->getFileName()));
        QMessageBox::warning(this, tr("Could not load configuration"), warn_text);
    }

    if (isVisible()) {
        config()->set("GUI/MainWindowGeometry", saveGeometry());
        config()->set("GUI/MainWindowState", saveState());
    }

    loadLanguage(config()->get("GUI/Language").toString());
    m_ui->CheckDeleteFiles->setChecked(config()->get("GUI/deleteFinished").toBool());
    switchTab(config()->get("GUI/currentIndexTab").toInt());
    m_ui->tabWidget->setCurrentIndex(config()->get("GUI/currentIndexTab").toInt());

    restoreGeometry(config()->get("GUI/MainWindowGeometry").toByteArray());
    restoreState(config()->get("GUI/MainWindowState").toByteArray());

    if (config()->get("GUI/darkTheme").toBool()) {
        m_ui->menuDarkTheme->setChecked(true);
        skin.setSkin("dark");
    } else {
        m_ui->menuDarkTheme->setChecked(false);
        skin.setSkin("notheme");
    }

    if (config()->get("GUI/showToolbar").toBool()) {
        m_ui->menuViewToolbar->setChecked(true);
        m_ui->toolBar->setVisible(true);
    } else {
        m_ui->menuViewToolbar->setChecked(false);
        m_ui->toolBar->setVisible(false);
    }

    if (config()->get("GUI/showPassword").toBool()) {
        m_ui->comboViewpass->setChecked(true);
        m_ui->password_0->setEchoMode(QLineEdit::Normal);
        m_ui->password_1->setEchoMode(QLineEdit::Normal);
    } else {
        m_ui->comboViewpass->setChecked(false);
        m_ui->password_0->setEchoMode(QLineEdit::Password);
        m_ui->password_1->setEchoMode(QLineEdit::Password);
    }
}

void MainWindow::savePreferences()
{
    if (isVisible()) {
        config()->set("GUI/MainWindowGeometry", saveGeometry());
        config()->set("GUI/MainWindowState", saveState());
    }
    config()->set("GUI/darkTheme", m_ui->menuDarkTheme->isChecked());
    config()->set("GUI/showPassword", m_ui->comboViewpass->isChecked());
    config()->set("GUI/Language", m_currLang);
    config()->set("GUI/showToolbar", m_ui->menuViewToolbar->isChecked());
    config()->set("GUI/currentIndexTab", m_ui->tabWidget->currentIndex());
    config()->set("GUI/deleteFinished", m_ui->CheckDeleteFiles->isChecked());
}

void MainWindow::aboutArsenic()
{
    auto* aboutDialog { new AboutDialog(this) };
    aboutDialog->open();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // save prefs before quitting
    savePreferences();
    if (Crypto->isRunning()) {
        Crypto->aborted = true;
        Crypto->wait();
    }
}

void MainWindow::quit() { close(); }

void MainWindow::configuration()
{
    auto* confDialog = new ConfigDialog(this);
    confDialog->exec();
}

void MainWindow::viewPassStateChanged(quint32 state)
{
    if (state == 0) {
        m_ui->password_0->setEchoMode(QLineEdit::Password);
        m_ui->password_1->setEchoMode(QLineEdit::Password);
    } else {
        m_ui->password_0->setEchoMode(QLineEdit::Normal);
        m_ui->password_1->setEchoMode(QLineEdit::Normal);
    }
}

void MainWindow::hashCalculator()
{
    auto* hashdlg = new HashCheckDialog(this);
    hashdlg->exec();
}

void MainWindow::Argon2_tests()
{
    auto* Argon2_tests = new ArgonTests(this);
    Argon2_tests->open();
}

void MainWindow::dark_theme(bool checked)
{
    if (checked) {
        skin.setSkin("dark");
    } else {
        skin.setSkin("notheme");
    }
}

// we create the menu entries dynamically, dependent on the existing
// translations.
void MainWindow::createLanguageMenu(void)
{
    QActionGroup* langGroup = new QActionGroup(m_ui->menuLanguage);
    langGroup->setExclusive(true);

    connect(langGroup, SIGNAL(triggered(QAction*)), this,
        SLOT(slotLanguageChanged(QAction*)));

    // format systems language
    auto defaultLocale { QLocale::system().name() }; // e.g. "de_DE"
    defaultLocale.truncate(defaultLocale.lastIndexOf('_')); // e.g. "de"

    m_langPath = QApplication::applicationDirPath();
    m_langPath.append("/languages");
    QDir dir(m_langPath);
    QStringList fileNames = dir.entryList(QStringList("arsenic_*.qm"));

    for (int i = 0; i < fileNames.size(); ++i) {
        // get locale extracted by filename
        QString locale;
        locale = fileNames[i]; // "TranslationExample_de.qm"
        locale.truncate(locale.lastIndexOf('.')); // "TranslationExample_de"
        locale.remove(0, locale.indexOf('_') + 1); // "de"

        QString lang = QLocale::languageToString(QLocale(locale).language());
        QIcon ico(QString("%1/%2.svg").arg(m_langPath).arg(locale));

        QAction* action = new QAction(ico, lang, this);
        action->setCheckable(true);
        action->setData(locale);

        m_ui->menuLanguage->addAction(action);
        langGroup->addAction(action);

        // set default translators and language checked
        if (config()->get("GUI/Language").toString() == locale) {
            action->setChecked(true);
        }
    }
}

// Called every time, when a menu entry of the language menu is called
void MainWindow::slotLanguageChanged(QAction* action)
{
    if (0 != action) {
        // load the language dependant on the action content
        loadLanguage(action->data().toString());
        // setWindowIcon(action->icon());
    }
}

void MainWindow::loadLanguage(const QString& rLanguage)
{
    if (m_currLang != rLanguage) {
        m_currLang = rLanguage;
        auto locale { QLocale(m_currLang) };
        QLocale::setDefault(locale);
        auto languageName = QLocale::languageToString(locale.language());
        switchTranslator(m_translator, QString("arsenic_%1.qm").arg(rLanguage));
        switchTranslator(m_translatorQt, QString("qt_%1.qm").arg(rLanguage));
        // ui->statusBar->showMessage(tr("Current Language changed to
        // %1").arg(languageName));
    }
}

void MainWindow::switchTranslator(QTranslator& translator,
    const QString& filename)
{
    // remove the old translator
    qApp->removeTranslator(&translator);

    // load the new translator
    auto path { QApplication::applicationDirPath() };
    path.append("/languages/");
    if (translator.load(path + filename)) // Here Path and Filename has to be entered because
        qApp->installTranslator(&translator); // the system didn't find the QM Files else
}

void MainWindow::changeEvent(QEvent* event)
{
    if (0 != event) {
        switch (event->type()) {
        // this event is send if a translator is loaded
        case QEvent::LanguageChange:
            m_ui->retranslateUi(this);
        default:
            break;
            break;

        // this event is send, if the system, language changes
        case QEvent::LocaleChange: {
            auto locale { QLocale::system().name() };
            locale.truncate(locale.lastIndexOf('_'));
            loadLanguage(locale);
        } break;
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::openTxtFile()
{
    if (maybeSave()) {
        auto fileName { QFileDialog::getOpenFileName(this) };
        if (!fileName.isEmpty())
            loadFile(fileName);
    }
}

void MainWindow::loadFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(
            this, tr("Read Error !"),
            tr("Cannot read file %1:\n%2.")
                .arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }

    QTextStream in(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    m_ui->cryptoPadEditor->setPlainText(in.readAll());
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    m_ui->statusBar->showMessage(tr("File loaded"), 2000);
}

void MainWindow::setCurrentFile(const QString& fileName)
{
    curFile = fileName;
    m_ui->cryptoPadEditor->document()->setModified(false);
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty())
        shownName = "untitled.txt";
    setWindowFilePath(shownName);
}

bool MainWindow::saveFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Write Error !"), tr("Cannot write file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return false;
    }

    QTextStream out(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    out << m_ui->cryptoPadEditor->toPlainText();
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    m_ui->statusBar->showMessage(tr("File saved"), 2000);
    out.flush();
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_ui->cryptoPadEditor->document()->isModified())
        return true;
    const QMessageBox::StandardButton ret = QMessageBox::warning(
        this, tr("Application"),
        tr("The document has been modified.\n"
           "Do you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return saveTxtFile();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

bool MainWindow::saveTxtFile()
{
    if (curFile.isEmpty()) {
        return saveTxtFileAs();
    } else {
        return saveFile(curFile);
    }
}

bool MainWindow::saveTxtFileAs()
{
    QFileDialog dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return saveFile(dialog.selectedFiles().first());
}

void MainWindow::encryptText()
{
    if (m_ui->cryptoPadEditor->toPlainText().isEmpty()) {
        displayEmptyEditor();
        return;
    }
    if (m_ui->password_0->text().isEmpty()) {
        displayEmptyPassword();
        return;
    }
    if (m_ui->password_0->text() != m_ui->password_1->text()) {
        displayPasswordNotMatch();
        return;
    }

    try {
        auto plaintext { m_ui->cryptoPadEditor->toPlainText() };
        auto out { encryptString(plaintext, m_ui->password_0->text()) };
        m_ui->cryptoPadEditor->setPlainText(out);
    } catch (std::exception const& e) {
        auto error = e.what();
        displayMessageBox(tr("Encryption Error!"), error);
    }
}

void MainWindow::decryptText()
{
    if (m_ui->cryptoPadEditor->toPlainText().isEmpty()) {
        displayEmptyEditor();
        return;
    }
    if (m_ui->password_0->text().isEmpty()) {
        displayEmptyPassword();
        return;
    }

    try {
        auto ciphertext { m_ui->cryptoPadEditor->toPlainText() };
        auto out { decryptString(ciphertext, m_ui->password_0->text()) };
        m_ui->cryptoPadEditor->setPlainText(out);
    } catch (std::exception const& e) {
        auto error = e.what();
        displayMessageBox(tr("Decryption Error!"), error);
    }
}

void MainWindow::displayMessageBox(QString title, QString text)
{
    QMessageBox::warning(this, (title), (text));
}

void MainWindow::displayPasswordNotMatch()
{
    QMessageBox::warning(this, tr("Passphrase do not match!"),
        tr("The passphrase fields do not match. Please make "
           "sure they were entered correctly and try again."));
}

void MainWindow::displayEmptyPassword()
{
    QMessageBox::warning(this, tr("Passphrase field is empty !"),
        tr("You must enter a passphrase."));
}

void MainWindow::displayEmptyJob()
{
    QMessageBox::warning(
        this, tr("Job list is empty !"),
        tr("You must add file(s) to the job list to start processing."));
}

void MainWindow::displayEmptyEditor()
{
    QMessageBox::warning(this, tr("Text editor is empty !"),
        tr("You must add text to editor to start processing."));
}
