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
#include <QScopedPointer>

#include "aboutDialog.h"
#include "Config.h"
#include "Delegate.h"
#include "configDialog.h"
#include "constants.h"
#include "loghtml.h"
#include "messages.h"
#include "passwordGeneratorDialog.h"
#include "hashcheckdialog.h"
#include "utils.h"
#include "argonTests.h"

using namespace ARs;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);
    m_file_crypto.reset(new Crypto_Thread);
    m_text_crypto.reset(new textCrypto);
    m_log.reset(new logHtml);

    createLanguageMenu();
    loadPreferences();
    loadLogFile();

    setWindowTitle(APP_LONG_NAME);
    cryptoFileView();
    delegate();

    // General

    // clang-format off
    connect(m_ui->menuAboutArsenic, &QAction::triggered, this,   [=] { aboutArsenic(); });
    connect(m_ui->menuHashCalculator, &QAction::triggered, this, [=] { hashCalculator(); });
    connect(m_ui->menuAboutQt, &QAction::triggered, this,        [=] { qApp->aboutQt(); });
    connect(m_ui->menuGenerator, &QAction::triggered, this,      [=] { generator(); });
    connect(m_ui->pushGenerator, &QPushButton::clicked, this,    [=] { generator(); });
    connect(m_ui->menuQuit, &QAction::triggered, this,           [=] { quit(); });
    connect(m_ui->menuQuit2, &QAction::triggered, this,          [=] { quit(); });
    connect(m_ui->menuQuit3, &QAction::triggered, this,          [=] { quit(); });
    connect(m_ui->menuConfiguration, &QAction::triggered, this,  [=] { configuration(); });
    connect(m_ui->menuArgon2Tests, &QAction::triggered, this,    [=] { Argon2_tests(); });
    connect(m_ui->menuDarkTheme, &QAction::triggered, this,      [=](const bool &checked) { dark_theme(checked); });
    connect(m_ui->menuViewToolbar, &QAction::triggered, this,    [=](const bool &checked) { m_ui->toolBar->setVisible(checked); });
    connect(m_ui->toolBar, &QToolBar::visibilityChanged, this,   [=](const bool &checked) { m_ui->menuViewToolbar->setChecked(checked); });
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this,  [=](const quint32 &index) { switchTab(index); });
    connect(m_ui->checkViewpass, &QCheckBox::stateChanged, this, [=](const quint32 &index) { viewPassStateChanged(index); });

    // EncryptPad
    connect(m_ui->menuOpenTxt, &QAction::triggered, this,        [=] { openTxtFile(); });
    connect(m_ui->menuSaveTxt, &QAction::triggered, this,        [=] { saveTxtFile(); });
    connect(m_ui->menuSaveTxtAs, &QAction::triggered, this,      [=] { saveTxtFileAs(); });
    connect(m_ui->menuEncryptTxt, &QAction::triggered, this,     [=] { encryptText(); });
    connect(m_ui->menuDecryptTxt, &QAction::triggered, this,     [=] { decryptText(); });
    connect(m_ui->menuClearEditor, &QAction::triggered, this,    [=] { clearEditor(); });
    connect(m_ui->pushEncryptTxt, &QPushButton::clicked, this,   [=] { encryptText(); });
    connect(m_ui->pushDecryptTxt, &QPushButton::clicked, this,   [=] { decryptText(); });

    // log
    connect(m_ui->menuClearLogView, &QAction::triggered, this,   [=] { clearLog(); });

    // EncryptFile
    connect(m_ui->menuAddFiles, &QAction::triggered, this,       [=] { addFiles(); });
    connect(m_ui->menuClearList, &QAction::triggered, this,      [=] { clearListFiles(); });
    connect(m_ui->menuEncryptList, &QAction::triggered, this,    [=] { encryptFiles(); });
    connect(m_ui->menuDecryptList, &QAction::triggered, this,    [=] { decryptFiles(); });
    connect(m_ui->pushEncrypt, &QPushButton::clicked, this,      [=] { encryptFiles(); });
    connect(m_ui->pushDecrypt, &QPushButton::clicked, this,      [=] { decryptFiles(); });
    connect(m_ui->menuAbortJob, &QAction::triggered, this,       [=] { abortJob(); });

    connect(m_file_crypto.data(), &Crypto_Thread::statusMessage, this,         [=](const QString &message) { onMessageChanged(message); });
    connect(m_file_crypto.data(), &Crypto_Thread::updateProgress, this,        [=](const QString &filename, const quint32 &progress) { onPercentProgress(filename, progress); });
    connect(m_file_crypto.data(), &Crypto_Thread::addEncrypted, this,          [=](const QString &filepath) { AddEncryptedFile(filepath); });
    connect(m_file_crypto.data(), &Crypto_Thread::addDecrypted, this,          [=](const QString &filepath) { AddDecryptedFile(filepath); });
    connect(m_file_crypto.data(), &Crypto_Thread::deletedAfterSuccess, this,   [=](const QString &filepath) { removeDeletedFile(filepath); });
    //connect(&pwGenerator, &PasswordGeneratorDialog::appliedPassword, this, [=](const QString &password) { setPassword(password); });
    //connect(&pwGenerator, SIGNAL(dialogTerminated()), &pwGenerator, SLOT(close()));
    // clang-format on
}

MainWindow::~MainWindow() {}

void MainWindow::session()
{
    // show the main window and load from the default item list
    show();
}

void MainWindow::removeDeletedFile(QString filepath)
{
    QList<QStandardItem *> items;
    QStandardItem *item;

    items = fileListModelCrypto->findItems(filepath, Qt::MatchExactly, 2);

    item       = items[0];
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
    m_ui->textLogs->setText(m_log->load());
}

void MainWindow::abortJob()
{
    if (m_file_crypto->isRunning()) {
        m_file_crypto->abort();
        m_file_crypto->wait();
    }
}

void MainWindow::onMessageChanged(QString message)
{
    QTextCursor c = m_ui->textLogs->textCursor();

    c.movePosition(QTextCursor::End);
    m_ui->textLogs->setTextCursor(c);
    m_ui->textLogs->append(message);
    m_log->append(m_ui->textLogs->toHtml());
}

void MainWindow::clearLog()
{
    m_ui->textLogs->clear();
    m_log->clear();
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
    m_file_crypto->setParam(true,
                            getListFiles(),
                            m_ui->password_0->text(),
                            config()->get("CRYPTO/argonMemory").toInt(),
                            config()->get("CRYPTO/argonItr").toInt(),
                            m_ui->CheckDeleteFiles->isChecked());

    m_file_crypto->start();
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
    m_file_crypto->setParam(false,
                            getListFiles(),
                            m_ui->password_0->text(),
                            config()->get("CRYPTO/argonMemory").toInt(),
                            config()->get("CRYPTO/argonItr").toInt(),
                            m_ui->CheckDeleteFiles->isChecked());

    m_file_crypto->start();
}

QStringList MainWindow::getListFiles()
{
    const auto rowCountCrypto = fileListModelCrypto->rowCount();
    QStringList fileList;
    if (0 < rowCountCrypto) {
        for (auto row = 0; row < rowCountCrypto; ++row) {
            QStandardItem *item = fileListModelCrypto->item(row, 2);
            fileList.append(item->text());
        }
    }
    return (fileList);
}

void MainWindow::clearEditor()
{
    m_ui->cryptoPadEditor->clear();
}

void MainWindow::onPercentProgress(const QString &path, quint32 percent)
{
    QList<QStandardItem *> items;
    QStandardItem *item;
    QStandardItem *progressItem;

    items = fileListModelCrypto->findItems(path, Qt::MatchExactly, 2);
    if (0 < items.size()) {
        item       = items[0];
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
        m_ui->checkViewpass->show();
        m_ui->password_label->show();
        m_ui->label->show();
        m_ui->pushGenerator->show();
    }
    if (index == 1) {
        m_ui->menuCryptopad->menuAction()->setVisible(true);
        m_ui->menuLog->menuAction()->setVisible(false);
        m_ui->menuFile->menuAction()->setVisible(false);
        m_ui->password_0->show();
        m_ui->password_1->show();
        m_ui->checkViewpass->show();
        m_ui->password_label->show();
        m_ui->label->show();
        m_ui->pushGenerator->show();
    }
    if (index == 2) {
        m_ui->menuLog->menuAction()->setVisible(true);
        m_ui->menuCryptopad->menuAction()->setVisible(false);
        m_ui->menuFile->menuAction()->setVisible(false);
        m_ui->password_0->hide();
        m_ui->password_1->hide();
        m_ui->checkViewpass->hide();
        m_ui->password_label->hide();
        m_ui->label->hide();
        m_ui->pushGenerator->hide();
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

    QHeaderView *headerCrypto = m_ui->fileListViewCrypto->horizontalHeader();

    headerCrypto->setStretchLastSection(true);

    // hide index numbers
    m_ui->fileListViewCrypto->verticalHeader()->setVisible(false);

    // Minimize all row height in Tables
    m_ui->fileListViewCrypto->verticalHeader()->setDefaultSectionSize(m_ui->fileListViewCrypto->verticalHeader()->minimumSectionSize());
}

void MainWindow::delegate()
{
    // Custom delegate paints progress bar and file close button for each file
    Delegate *delegate = new Delegate{this};

    m_ui->fileListViewCrypto->setItemDelegate(delegate);
    m_ui->fileListViewCrypto->setModel(fileListModelCrypto);

    connect(delegate, &Delegate::removeRow, this, &MainWindow::removeFile);
}

void MainWindow::addFiles()
{
    // Open a file dialog to get files
    const auto files = QFileDialog::getOpenFileNames(this, tr("Add File(s)"), config()->get("GUI/lastDirectory").toByteArray());
    if (files.isEmpty()) // if no file selected
    {
        return;
    }
    // Save this directory to return to later
    const auto fileName{files[0]};

    config()->set("GUI/lastDirectory", fileName.left(fileName.lastIndexOf("/")));
    for (const QString &file : files) // add files to the model
    {
        addFilePathToModel(file);
    }
}

void MainWindow::addFilePathToModel(const QString &filePath)
{
    QFileInfo fileInfo{filePath};
    if (fileInfo.exists() && fileInfo.isFile()) // If the file exists, add it to the model
    {
        const auto fileSize{Utils::getFileSize((fileInfo.size()))};
        const auto fileName{fileInfo.fileName()};

        const auto fileItem{new QStandardItem{fileName}};
        fileItem->setDragEnabled(false);
        fileItem->setDropEnabled(false);
        fileItem->setEditable(false);
        fileItem->setSelectable(false);
        fileItem->setToolTip(fileName);
        const auto fileVariant{QVariant::fromValue(fileName)};
        fileItem->setData(fileVariant);

        const auto pathItem{new QStandardItem{filePath}};
        pathItem->setDragEnabled(false);
        pathItem->setDropEnabled(false);
        pathItem->setEditable(false);
        pathItem->setSelectable(false);
        pathItem->setToolTip(filePath);
        const auto pathVariant{QVariant::fromValue(filePath)};
        pathItem->setData(pathVariant);

        const auto sizeItem{new QStandardItem{fileSize}};
        sizeItem->setDragEnabled(false);
        sizeItem->setDropEnabled(false);
        sizeItem->setEditable(false);
        sizeItem->setSelectable(false);
        sizeItem->setToolTip(QString::number(fileInfo.size()) + " bytes");
        const auto sizeVariant{QVariant::fromValue(fileSize)};
        sizeItem->setData(sizeVariant);

        const auto progressItem{new QStandardItem{}};
        progressItem->setDragEnabled(false);
        progressItem->setDropEnabled(false);
        progressItem->setEditable(false);
        progressItem->setSelectable(false);

        const auto closeFileItem{new QStandardItem{}};
        closeFileItem->setDragEnabled(false);
        closeFileItem->setDropEnabled(false);
        closeFileItem->setEditable(false);
        closeFileItem->setSelectable(false);

        const QList<QStandardItem *> items = {closeFileItem, fileItem, pathItem, sizeItem, progressItem};

        // Search to see if this item is already in the model
        auto addNewItem = true;

        const auto rowCryptoCount = fileListModelCrypto->rowCount();
        for (auto row = 0; row < rowCryptoCount; ++row) {
            auto testItem = fileListModelCrypto->item(row, 1);
            if (testItem->data().toString() == pathItem->data().toString()) {
                addNewItem = false;
            }
        }
        if (addNewItem) // Add the item to the model if it's new
        {
            fileListModelCrypto->appendRow(items);
        }
    }
    m_ui->fileListViewCrypto->resizeColumnsToContents();
    m_ui->fileListViewCrypto->setColumnWidth(2, 100); // Limit the "path" to 100
    // px

    // End if file exists and is a file
}

void MainWindow::removeFile(const QModelIndex &index)
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
    auto pwGenerator{new PasswordGeneratorDialog};

    pwGenerator->setStandaloneMode(true);
    connect(pwGenerator, SIGNAL(appliedPassword(QString)), SLOT(setPassword(QString)));
    connect(pwGenerator, SIGNAL(dialogTerminated()), pwGenerator, SLOT(close()));
    pwGenerator->setStandaloneMode(true);
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
    }
    else {
        m_ui->menuDarkTheme->setChecked(false);
        skin.setSkin("notheme");
    }
    if (config()->get("GUI/showToolbar").toBool()) {
        m_ui->menuViewToolbar->setChecked(true);
        m_ui->toolBar->setVisible(true);
    }
    else {
        m_ui->menuViewToolbar->setChecked(false);
        m_ui->toolBar->setVisible(false);
    }
    if (config()->get("GUI/showPassword").toBool()) {
        m_ui->checkViewpass->setChecked(true);
        m_ui->password_0->setEchoMode(QLineEdit::Normal);
        m_ui->password_1->setEchoMode(QLineEdit::Normal);
    }
    else {
        m_ui->checkViewpass->setChecked(false);
        m_ui->password_0->setEchoMode(QLineEdit::Password);
        m_ui->password_1->setEchoMode(QLineEdit::Password);
    }
}

void MainWindow::savePreferences()
{
    // clang-format off
    if (isVisible()) {
        config()->set("GUI/MainWindowGeometry", saveGeometry());
        config()->set("GUI/MainWindowState",    saveState());
    }
    config()->set("GUI/darkTheme",       m_ui->menuDarkTheme->isChecked());
    config()->set("GUI/showPassword",    m_ui->checkViewpass->isChecked());
    config()->set("GUI/Language",        m_currLang);
    config()->set("GUI/showToolbar",     m_ui->menuViewToolbar->isChecked());
    config()->set("GUI/currentIndexTab", m_ui->tabWidget->currentIndex());
    config()->set("GUI/deleteFinished",  m_ui->CheckDeleteFiles->isChecked());
    // clang-format on
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    // save prefs before quitting
    savePreferences();
    if (m_file_crypto->isRunning()) {
        m_file_crypto->abort();
        m_file_crypto->wait();
    }
}

void MainWindow::quit()
{
    close();
}

void MainWindow::configuration()
{
    auto *confDialog = new ConfigDialog(this);
    confDialog->exec();
}

void MainWindow::viewPassStateChanged(quint32 state)
{
    if (state == 0) {
        m_ui->password_0->setEchoMode(QLineEdit::Password);
        m_ui->password_1->setEchoMode(QLineEdit::Password);
    }
    else {
        m_ui->password_0->setEchoMode(QLineEdit::Normal);
        m_ui->password_1->setEchoMode(QLineEdit::Normal);
    }
}

void MainWindow::hashCalculator()
{
    auto *m_hashDialog = new HashCheckDialog(this);
    m_hashDialog->open();
}

void MainWindow::aboutArsenic()
{
    auto *aboutDialog = new AboutDialog(this);
    aboutDialog->open();
}

void MainWindow::Argon2_tests()
{

    auto *argon2_tests_Dialog = new ArgonTests(this);
    argon2_tests_Dialog->exec();
}

void MainWindow::dark_theme(bool checked)
{
    if (checked) {
        skin.setSkin("dark");
    }
    else {
        skin.setSkin("notheme");
    }
}

// we create the menu entries dynamically, dependent on the existing
// translations.
void MainWindow::createLanguageMenu(void)
{
    QActionGroup *langGroup = new QActionGroup(m_ui->menuLanguage);
    langGroup->setExclusive(true);

    connect(langGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotLanguageChanged(QAction *)));

    // format systems language
    auto defaultLocale{QLocale::system().name()}; // e.g. "de_DE"

    defaultLocale.truncate(defaultLocale.lastIndexOf('_')); // e.g. "de"

#ifdef Q_OS_WIN
    m_langPath = QApplication::applicationDirPath();
    m_langPath.append("/languages");
#endif
#ifdef Q_OS_UNIX
    m_langPath = "/usr/share/arsenic/languages/";
#endif
    QDir dir(m_langPath);
    QStringList fileNames = dir.entryList(QStringList("arsenic_*.qm"));
    for (int i = 0; i < fileNames.size(); ++i) {
        // get locale extracted by filename
        QString locale;
        locale = fileNames[i];                     // "TranslationExample_de.qm"
        locale.truncate(locale.lastIndexOf('.'));  // "TranslationExample_de"
        locale.remove(0, locale.indexOf('_') + 1); // "de"

        QString lang = QLocale::languageToString(QLocale(locale).language());
        QIcon ico(QString("%1/%2.svg").arg(m_langPath).arg(locale));

        QAction *action = new QAction(ico, lang, this);
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
void MainWindow::slotLanguageChanged(QAction *action)
{
    if (0 != action) {
        // load the language dependant on the action content
        loadLanguage(action->data().toString());
        // setWindowIcon(action->icon());
    }
}

void MainWindow::loadLanguage(const QString &rLanguage)
{
    if (m_currLang != rLanguage) {
        m_currLang = rLanguage;
        auto locale{QLocale(m_currLang)};
        QLocale::setDefault(locale);
        auto languageName = QLocale::languageToString(locale.language());
        switchTranslator(m_translator, QString("arsenic_%1.qm").arg(rLanguage));
        switchTranslator(m_translatorQt, QString("qt_%1.qm").arg(rLanguage));
        // ui->statusBar->showMessage(tr("Current Language changed to
        // %1").arg(languageName));
    }
}

void MainWindow::switchTranslator(QTranslator &translator, const QString &filename)
{
    // remove the old translator
    qApp->removeTranslator(&translator);
#ifdef Q_OS_WIN
    // load the new translator
    auto path{QApplication::applicationDirPath()};
    path.append("/languages/");
#endif
#ifdef Q_OS_UNIX
    auto path = "/usr/share/arsenic/languages/";
#endif
    if (translator.load(path + filename)) // Here Path and Filename has to be entered because
    {
        qApp->installTranslator(&translator); // the system didn't find the QM Files else
    }
}

void MainWindow::changeEvent(QEvent *event)
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
                auto locale{QLocale::system().name()};
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
        auto fileName{QFileDialog::getOpenFileName(this)};
        if (!fileName.isEmpty()) {
            loadFile(fileName);
        }
    }
}

void MainWindow::loadFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Read Error !"), tr("Cannot read file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
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

void MainWindow::setCurrentFile(const QString &fileName)
{
    curFile = fileName;
    m_ui->cryptoPadEditor->document()->setModified(false);
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty()) {
        shownName = "untitled.txt";
    }
    setWindowFilePath(shownName);
}

bool MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Write Error !"), tr("Cannot write file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return (false);
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
    return (true);
}

bool MainWindow::maybeSave()
{
    if (!m_ui->cryptoPadEditor->document()->isModified()) {
        return (true);
    }
    const QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("Application"),
                                                                 tr("The document has been modified.\n"
                                                                    "Do you want to save your changes?"),
                                                                 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
        case QMessageBox::Save:
            return (saveTxtFile());

        case QMessageBox::Cancel:
            return (false);

        default:
            break;
    }
    return (true);
}

bool MainWindow::saveTxtFile()
{
    if (curFile.isEmpty()) {
        return (saveTxtFileAs());
    }
    else {
        return (saveFile(curFile));
    }
}

bool MainWindow::saveTxtFileAs()
{
    QFileDialog dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec() != QDialog::Accepted) {
        return (false);
    }
    return (saveFile(dialog.selectedFiles().first()));
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
    auto plaintext{m_ui->cryptoPadEditor->toPlainText()};

    m_text_crypto->start(m_ui->password_0->text(), true);
    quint32 result = m_text_crypto->finish(plaintext);
    if (result != CRYPT_SUCCESS) {
        displayMessageBox(tr("Encryption Error!"), errorCodeToString(result));
    }
    else {
        m_ui->cryptoPadEditor->setPlainText(plaintext);
    }
}

void MainWindow::decryptText()
{
    std::vector<int> const tableau_entiers{1, 3, 5, 7, 9};
    std::vector<QString> const tableau_qstring{"hfhgfh", "hyuyu", "5.123"};

    Utils::afficher(tableau_qstring);
    Utils::afficher(tableau_entiers);

    if (m_ui->cryptoPadEditor->toPlainText().isEmpty()) {
        displayEmptyEditor();
        return;
    }
    if (m_ui->password_0->text().isEmpty()) {
        displayEmptyPassword();
        return;
    }
    auto ciphertext{m_ui->cryptoPadEditor->toPlainText()};
    m_text_crypto->start(m_ui->password_0->text(), false);

    quint32 result = m_text_crypto->finish(ciphertext);
    if (result != DECRYPT_SUCCESS) {
        displayMessageBox(tr("Decryption Error!"), errorCodeToString(result));
    }
    else {
        m_ui->cryptoPadEditor->setPlainText(ciphertext);
    }
}

void MainWindow::displayMessageBox(const QString &title, const QString &text)
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
    QMessageBox::warning(this, tr("Job list is empty !"),
                         tr("You must add file(s) to the job list to start processing."));
}

void MainWindow::displayEmptyEditor()
{
    QMessageBox::warning(this, tr("Text editor is empty !"),
                         tr("You must add text to editor to start processing."));
}
