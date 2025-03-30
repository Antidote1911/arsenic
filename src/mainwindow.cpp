
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

#include "aboutDialog.h"
#include "Config.h"
#include "loghtml.h"
#include "Delegate.h"
#include "configDialog.h"
#include "arscore/messages.h"
#include "passwordGeneratorDialog.h"
#include "hashcheckdialog.h"
#include "arscore/utils.h"
#include "argonTests.h"
#include "skin/skin.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()),
      m_file_crypto(std::make_unique<Crypto_Thread>(this)),
      m_text_crypto(std::make_unique<textCrypto>(this))
{
    m_ui->setupUi(this);

    //m_file_crypto = std::make_unique<Crypto_Thread>();
    //m_text_crypto = std::make_unique<textCrypto>();
    //m_log         = std::make_unique<logHtml>();
    //m_skin        = std::make_unique<Skin>();

    loadPreferences();
    loadLogFile();
    initViewMenu();
    applyTheme();

    setWindowTitle(m_const->APP_LONG_NAME);
    cryptoFileView();
    delegate();

    // General
    // clang-format off
    connect(m_ui->menuAboutArsenic, &QAction::triggered, this,   [=, this] { aboutArsenic(); });
    connect(m_ui->menuHashCalculator, &QAction::triggered, this, [=, this] { hashCalculator(); });
    connect(m_ui->menuAboutQt, &QAction::triggered, this,        [=] { qApp->aboutQt(); });
    connect(m_ui->menuGenerator, &QAction::triggered, this,      [=, this] { generator(); });
    connect(m_ui->pushGenerator, &QPushButton::clicked, this,    [=, this] { generator(); });
    connect(m_ui->menuQuit, &QAction::triggered, this,           [=, this] { quit(); });
    connect(m_ui->menuQuit2, &QAction::triggered, this,          [=, this] { quit(); });
    connect(m_ui->menuQuit3, &QAction::triggered, this,          [=, this] { quit(); });
    connect(m_ui->menuConfiguration, &QAction::triggered, this,  [=, this] { configuration(); });
    connect(m_ui->menuArgon2Tests, &QAction::triggered, this,    [=, this] { Argon2_tests(); });
    connect(m_ui->menuViewToolbar, &QAction::triggered, this,    [=, this](const bool &checked) { m_ui->toolBar->setVisible(checked); });
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this,  [=, this](const quint32 &index) { switchTab(index); });
    connect(m_ui->checkViewpass, &QCheckBox::checkStateChanged, this, [=, this](const quint32 &index) { viewPassStateChanged(index); });

    // EncryptPad
    connect(m_ui->menuOpenTxt, &QAction::triggered, this,        [=, this] { openTxtFile(); });
    connect(m_ui->menuSaveTxt, &QAction::triggered, this,        [=, this] { saveTxtFile(); });
    connect(m_ui->menuSaveTxtAs, &QAction::triggered, this,      [=, this] { saveTxtFileAs(); });
    connect(m_ui->menuEncryptTxt, &QAction::triggered, this,     [=, this] { encryptText(); });
    connect(m_ui->menuDecryptTxt, &QAction::triggered, this,     [=, this] { decryptText(); });
    connect(m_ui->menuClearEditor, &QAction::triggered, this,    [=, this] { clearEditor(); });
    connect(m_ui->pushEncryptTxt, &QPushButton::clicked, this,   [=, this] { encryptText(); });
    connect(m_ui->pushDecryptTxt, &QPushButton::clicked, this,   [=, this] { decryptText(); });

    // log
    connect(m_ui->menuClearLogView, &QAction::triggered, this,   [=, this] { clearLog(); });

    // EncryptFile
    connect(m_ui->menuAddFiles, &QAction::triggered, this,       [=, this] { addFiles(); });
    connect(m_ui->menuAddFolder, &QAction::triggered, this,       [=, this] { addFolder(); });
    connect(m_ui->menuClearList, &QAction::triggered, this,      [=, this] { clearListFiles(); });
    connect(m_ui->menuEncryptList, &QAction::triggered, this,    [=, this] { encryptFiles(); });
    connect(m_ui->menuDecryptList, &QAction::triggered, this,    [=, this] { decryptFiles(); });
    connect(m_ui->pushEncrypt, &QPushButton::clicked, this,      [=, this] { encryptFiles(); });
    connect(m_ui->pushDecrypt, &QPushButton::clicked, this,      [=, this] { decryptFiles(); });
    connect(m_ui->menuAbortJob, &QAction::triggered, this,       [=, this] { abortJob(); });

    connect(m_file_crypto.get(), &Crypto_Thread::statusMessage, this,         [=, this](const QString &message) { onMessageChanged(message); });
    connect(m_file_crypto.get(), &Crypto_Thread::updateProgress, this,        [=, this](const QString &filename, const quint32 &progress) { onPercentProgress(filename, progress); });
    connect(m_file_crypto.get(), &Crypto_Thread::addEncrypted, this,          [=, this](const QString &filepath) { AddEncryptedFile(filepath); });
    connect(m_file_crypto.get(), &Crypto_Thread::deletedAfterSuccess, this,   [=, this](const QString &filepath) { removeDeletedFile(filepath); });


    //connect(&pwGenerator, &PasswordGeneratorDialog::appliedPassword, this, [=](const QString &password) { setPassword(password); });
    //connect(&pwGenerator, SIGNAL(dialogTerminated()), &pwGenerator, SLOT(close()));
    // clang-format on
}

MainWindow::~MainWindow() {}

void MainWindow::initViewMenu()
{
    setContextMenuPolicy(Qt::NoContextMenu);

    m_ui->actionThemeDark->setData("dark");
    m_ui->actionThemeClassic->setData("classic");
    m_ui->actionThemeLight->setData("light");

    auto themeActions = new QActionGroup(this);
    themeActions->addAction(m_ui->actionThemeDark);
    themeActions->addAction(m_ui->actionThemeClassic);
    themeActions->addAction(m_ui->actionThemeLight);

    auto theme = config()->get(Config::GUI_ApplicationTheme).toString();
    for (auto action : themeActions->actions()) {
        if (action->data() == theme) {
            action->setChecked(true);
            break;
        }
    }

    connect(themeActions, &QActionGroup::triggered, this, [this](QAction *action) {
        if (action->data() != config()->get(Config::GUI_ApplicationTheme)) {
            config()->set(Config::GUI_ApplicationTheme, action->data());
            restartApp();
        }
    });
}

void MainWindow::restartApp()
{
    int ret = QMessageBox::question(this, tr("Restart Application ?"),
                                    tr("To take effect, Arsenic need to be restarted.\n"
                                       "Do you want to restart now ?"),
                                    QMessageBox::No | QMessageBox::Yes,
                                    QMessageBox::Yes);

    if (ret == QMessageBox::Yes) {
        close();
        reboot();
    }
}

void MainWindow::applyTheme()
{
    QString appTheme = config()->get(Config::GUI_ApplicationTheme).toString();
    if (appTheme == "classic") {
        skin()->setSkin("classic");
    }
    else if (appTheme == "dark") {
        skin()->setSkin("dark");
    }
    else {
        skin()->setSkin("light");
    }

    //setPalette(style()->standardPalette());
}

void MainWindow::removeDeletedFile(QString filepath)
{
    QList<QStandardItem *> items;
    QStandardItem *item;
    items      = fileListModelCrypto->findItems(filepath, Qt::MatchExactly, 2);
    item       = items[0];
    auto index = item->row();
    if (fileListModelCrypto->hasChildren()) {
        fileListModelCrypto->removeRow(index);
    }
}

void MainWindow::AddEncryptedFile(QString filepath)
{
    if (config()->get(Config::GUI_AddEncrypted).toBool()) {
        addFilePathToModel(filepath);
    }
}

void MainWindow::loadLogFile()
{
    m_ui->textLogs->setText(loghtml()->load());
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
    loghtml()->append(m_ui->textLogs->toHtml());
}

void MainWindow::clearLog()
{
    m_ui->textLogs->clear();
    loghtml()->clear();
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

    auto algo = config()->get(Config::CRYPTO_algorithm).toString();



        m_file_crypto->setParam(true,
                                getListFiles(),
                                m_ui->password_0->text(),
                                algo,
                                config()->get(Config::CRYPTO_argonMemory).toInt(),
                                config()->get(Config::CRYPTO_argonItr).toInt(),
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

    auto algo = config()->get(Config::CRYPTO_algorithm).toString();
    m_file_crypto->setParam(false,
                            getListFiles(),
                            m_ui->password_0->text(),
                            algo,
                            config()->get(Config::CRYPTO_argonMemory).toInt(),
                            config()->get(Config::CRYPTO_argonItr).toInt(),
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
    fileListModelCrypto = std::make_unique<QStandardItemModel>(0, 5);
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

    m_delegate = std::make_unique<Delegate>();

    m_ui->fileListViewCrypto->setItemDelegate(m_delegate.get());
    m_ui->fileListViewCrypto->setModel(fileListModelCrypto.get());

    connect(m_delegate.get(), &Delegate::removeRow, this, &MainWindow::removeFile);
}

void MainWindow::addFiles()
{
    // Open a file dialog to get files
    const auto files = QFileDialog::getOpenFileNames(this, tr("Add File(s)"), config()->get(Config::GUI_lastDirectory).toByteArray());
    if (files.isEmpty()) // if no file selected
    {
        return;
    }
    // Save this directory to return to later
    const auto fileName{files[0]};

    config()->set(Config::GUI_lastDirectory, fileName.left(fileName.lastIndexOf("/")));
    for (const QString &file : files) // add files to the model
    {
        addFilePathToModel(file);
    }
}

void MainWindow::addFolder()
{
    // Open a file dialog to get files
    const auto directory = QFileDialog::getExistingDirectory(this, tr("Add Folder"), config()->get(Config::GUI_lastDirectory).toByteArray());
    addFilePathToModel(directory);
}

void MainWindow::addFilePathToModel(const QString &filePath)
{
    QFileInfo fileInfo{filePath};
    QString fileSize;
    qint64 dirSize;
    if (fileInfo.exists() && fileInfo.isFile() | fileInfo.isDir()) // If the file exists, add it to the model
    {
        if (fileInfo.isDir()) {

            dirSize  = Utils::getDirSize(filePath);
            fileSize = Utils::getFileSize(dirSize);
        }
        else {
            fileSize = Utils::getFileSize(fileInfo.size());
        }

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
        if (fileInfo.isDir()) {
            sizeItem->setToolTip(QString::number(dirSize) + " bytes");
        }
        else {
            sizeItem->setToolTip(QString::number(fileInfo.size()) + " bytes");
        }

        const auto sizeVariant{QVariant::fromValue(fileSize)};
        sizeItem->setData(sizeVariant);

        const auto closeFileItem{new QStandardItem{}};
        closeFileItem->setDragEnabled(false);
        closeFileItem->setDropEnabled(false);
        closeFileItem->setEditable(false);
        closeFileItem->setSelectable(false);

        const auto progressItem{new QStandardItem{}};
        progressItem->setDragEnabled(false);
        progressItem->setDropEnabled(false);
        progressItem->setEditable(false);
        progressItem->setSelectable(false);

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
    m_ui->fileListViewCrypto->setColumnWidth(2, 100); // Limit the "path" to 100 px

    // End if file exists and is a file
}

void MainWindow::removeFile(const QModelIndex &index)
{
    abortJob();
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
    auto pwGenerator = new PasswordGeneratorWidget;

    //pwGenerator->setStandaloneMode(false);
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

    m_ui->CheckDeleteFiles->setChecked(config()->get(Config::GUI_deleteFinished).toBool());
    switchTab(config()->get(Config::GUI_currentIndexTab).toInt());
    m_ui->tabWidget->setCurrentIndex(config()->get(Config::GUI_currentIndexTab).toInt());
    m_ui->menuViewToolbar->setChecked(config()->get(Config::GUI_showToolbar).toBool());
    restoreGeometry(config()->get(Config::GUI_MainWindowGeometry).toByteArray());
    restoreState(config()->get(Config::GUI_MainWindowState).toByteArray());

    if (config()->get(Config::GUI_showPassword).toBool()) {
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
        config()->set(Config::GUI_MainWindowGeometry, saveGeometry());
        config()->set(Config::GUI_MainWindowState,    saveState());
    }
    config()->set(Config::GUI_showToolbar, m_ui->menuViewToolbar->isChecked());
    config()->set(Config::GUI_showPassword,    m_ui->checkViewpass->isChecked());
    config()->set(Config::GUI_currentIndexTab, m_ui->tabWidget->currentIndex());
    config()->set(Config::GUI_deleteFinished,  m_ui->CheckDeleteFiles->isChecked());
    // clang-format on
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    // save prefs before quitting
    savePreferences();
    abortJob();
}

void MainWindow::quit()
{
    close();
}

void MainWindow::reboot()
{
    qDebug() << "Performing application reboot...";
    qApp->exit(m_const->EXIT_CODE_REBOOT);
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
    aboutDialog->exec();
}

void MainWindow::Argon2_tests()
{

    auto *argon2_tests_Dialog = new ArgonTests(this);
    argon2_tests_Dialog->exec();
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
    quint32 result = m_text_crypto.get()->finish(plaintext);
    if (result != CRYPT_SUCCESS) {
        displayMessageBox(tr("Encryption Error!"), errorCodeToString(result));
    }
    else {
        m_ui->cryptoPadEditor->setPlainText(plaintext);
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
