#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Delegate.h"
#include "preferences.h"
#include "config.h"
#include "about.h"

#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QDateTime>
#include <QTextStream>
#include <QTextCodec>
#include <QTranslator>
#include <QTableWidget>
#include <QSettings>
#include <iostream>
#include <QStandardItemModel>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
    //,m_prefs()

{
    ui->setupUi(this);
    MainWindow::setWindowTitle(ARs::APP_NAME);

    Crypto = new Crypto_Thread(this);
    Hash   = new Hash_Tool(this);

    connect(Crypto,SIGNAL(PercentProgressChanged(QString, qint64,QString)),this,SLOT(onPercentProgress(QString, qint64,QString)));
    connect(Hash,SIGNAL(PercentProgressHashChanged(QString,qint64,QString)),this,SLOT(onPercentProgress(QString,qint64,QString)));

    connect(Crypto,SIGNAL(statusMessage(QString,QString,QString)),this,SLOT(onMessageChanged(QString,QString,QString)));
    connect(Hash,SIGNAL(statusResult(QString,QString,QString)),this,SLOT(onResultHashChanged(QString,QString,QString)));

    loadOptions();
    restoreGeomAndUi();
    restoreLogText();
    cryptoFileView();
    cryptoHashView();
    delegate();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadOptions()
{
    QSettings settings;
    m_prefs.algoCrypto        = settings.value("algo_crypto").toString();
    m_prefs.algoHash          = settings.value("algo_hash").toString();
    m_prefs.encoding          = settings.value("encoding").toString();
    m_prefs.lastDirectory     = settings.value("lastDirectory").toString();
    m_prefs.deleteOriginals   = settings.value("deleteOriginals").toBool();
    m_prefs.restoreGeometry   = settings.value("mainWindowGeometry").toByteArray();
    m_prefs.restoreState      = settings.value("mainWindowState").toByteArray();
    m_prefs.cipherDirection   = settings.value("cipherDirection").toInt();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("algo_crypto", m_prefs.algoCrypto);
    settings.setValue("algo_hash", m_prefs.algoHash);
    settings.setValue("encoding", m_prefs.encoding);
    settings.setValue("lastDirectory", m_prefs.lastDirectory);
    settings.setValue("deleteOriginals",m_prefs.deleteOriginals);
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("cipherDirection", m_prefs.cipherDirection);
}

void MainWindow::restoreGeomAndUi()
{
    restoreGeometry(m_prefs.restoreGeometry);
    restoreState(m_prefs.restoreState);
    ui->comboHash->setCurrentText(m_prefs.algoHash);
    ui->comboDirection->setCurrentIndex(m_prefs.cipherDirection);
}

void MainWindow::restoreLogText()
{
    //read log file and display content as html
    QFile file("arsenic.log.html");
    if (!file.open(QFile::ReadOnly))
    {
        file.open(QFile::WriteOnly);
        file.close();
    }
    QByteArray data = file.readAll();
    QTextCodec *codec = Qt::codecForHtml(data);
    QString log = codec->toUnicode(data);
    ui->plainTextLog->setHtml(log);
}

void MainWindow::cryptoFileView()
{
    fileListModelCrypto = new QStandardItemModel(0,5);
    fileListModelCrypto->setHeaderData(0, Qt::Horizontal, tr("File"));
    fileListModelCrypto->setHeaderData(1, Qt::Horizontal, tr("Path"));
    fileListModelCrypto->setHeaderData(2, Qt::Horizontal, tr("Size in Mb"));
    fileListModelCrypto->setHeaderData(3, Qt::Horizontal, tr("Progress"));
    fileListModelCrypto->setHeaderData(4, Qt::Horizontal, tr("Remove File"));

    QHeaderView* headerCrypto = ui->fileListViewCrypto->horizontalHeader();
    headerCrypto->setStretchLastSection(false);
}

void MainWindow::cryptoHashView()
{
    fileListModelHash = new QStandardItemModel(0,5);
    fileListModelHash->setHeaderData(0, Qt::Horizontal, tr("File"));
    fileListModelHash->setHeaderData(1, Qt::Horizontal, tr("Path"));
    fileListModelHash->setHeaderData(2, Qt::Horizontal, tr("Size in Mb"));
    fileListModelHash->setHeaderData(3, Qt::Horizontal, tr("Progress"));
    fileListModelHash->setHeaderData(4, Qt::Horizontal, tr("Remove File"));

    QHeaderView* headerHash = ui->fileListViewHash->horizontalHeader();
    headerHash->setStretchLastSection(false);
}

void MainWindow::delegate()
{
    // Custom delegate paints progress bar and file close button for each file
    Delegate* delegate = new Delegate{this};

    ui->fileListViewCrypto->setItemDelegate(delegate);
    ui->fileListViewCrypto->setModel(fileListModelCrypto);

    ui->fileListViewHash->setItemDelegate(delegate);
    ui->fileListViewHash->setModel(fileListModelHash);

    connect(delegate, &Delegate::removeRow, this, &MainWindow::removeFile);
}

void MainWindow::removeFile(const QModelIndex& index)
{
    if (ui->tabWidget->currentIndex()==0)
    {
        fileListModelCrypto->removeRow(index.row());
    }
    else
    {
        fileListModelHash->removeRow(index.row());
    }
}

void MainWindow::onResultHashChanged(QString algo, QString message, QString filename)
{
    ui->plainTextHash->appendPlainText(filename);
    ui->plainTextHash->appendPlainText("Hash algo: " + algo);
    ui->plainTextHash->appendPlainText(message);
    ui->plainTextHash->appendPlainText("--------------------------");
    ui->pushStartHash->setDisabled(false);

    ui->comboHash->setDisabled(false);
    onMessageChanged(algo, message,filename);
}


void MainWindow::onMessageChanged(QString algo, QString message, QString filename)
{
    QTextCursor c =  ui->plainTextLog->textCursor();
    c.movePosition(QTextCursor::End);
    ui->plainTextLog->setTextCursor(c);

    ui->plainTextLog->append("---------------------- "+QDateTime::currentDateTime().toString("dddd dd MMMM yyyy (hh:mm:ss)")+" ----------------------");
    ui->plainTextLog->append(algo);
    ui->plainTextLog->append(message);
    ui->plainTextLog->append(filename);
    ui->plainTextLog->append("");

    c.movePosition(QTextCursor::End);
    ui->plainTextLog->setTextCursor(c);

    QFile outfile;
    outfile.setFileName("arsenic.log.html");
    outfile.open(QIODevice::ReadWrite| QIODevice::Text);
    QTextStream out(&outfile);
    out << ui->plainTextLog->toHtml() << endl;
    outfile.close();
}

void MainWindow::onPercentProgress(const QString& path, qint64 percent, QString source)
{
    QList<QStandardItem*> items;
    QStandardItem* item;
    QStandardItem* progressItem;

    if(source=="crypto_object")
    {
        items = fileListModelCrypto->findItems(path,Qt::MatchExactly,1);
    }
    else
    {
        items = fileListModelHash->findItems(path,Qt::MatchExactly,1);
    }

    if (0 < items.size())
    {
        item = items[0];
        int index = item->row();

        if (nullptr != item)
        {

            if(source=="crypto_object")
            {
                progressItem = fileListModelCrypto->item(index, 3);
            }
            else
            {
                progressItem = fileListModelHash->item(index, 3);
            }

            if (nullptr != progressItem)
            {
                progressItem->setData(percent, Qt::DisplayRole);
            }
        }
    }
}

void MainWindow::on_pushAddCrypto_clicked()
{
    QString cryptoOrHash="crypto";
    addFiles(cryptoOrHash);
}

void MainWindow::on_pushAddHash_clicked()
{
    QString cryptoOrHash="hash";
    addFiles(cryptoOrHash);
}

void MainWindow::addFiles(QString cryptoOrHash)
{
    // Open a file dialog to get files
    const QStringList files = QFileDialog::getOpenFileNames(this,tr("Add File(s)"), m_prefs.lastDirectory,
                              tr("All (*.*) ;; "
                                 "Arsenic Files(*.ars) ;; "
                                 "Text Files (*.txt *.doc *.inf *.ini *.rtf *.html .*css) ;; "
                                 "Images (*.jpg *.jpeg *.bmp *.tif *.svg *.tga *.png *.gif *.psd *.xcf) ;; "
                                 "Videos (*.avi *.mkv *.mpg *.mpeg *.webm *.divx *.mov *.3gp *.rm *.flv *.wmv *.vob *.ts *.ogg *.ogm) ;; "
                                 "Executables (*.exe *.bat)"));


    if (files.isEmpty())               // if no file selected
        return;

    // Save this directory to return to later
    const QString fileName = files[0];
    m_prefs.lastDirectory = fileName.left(fileName.lastIndexOf("/"));

    for (const QString& file : files)  // add files to the model
    {
        addFilePathToModel(file,cryptoOrHash);
    }
}

void MainWindow::addFilePathToModel(const QString& filePath,QString cryptoOrHash)
{
    QFileInfo fileInfo{filePath};

    if (fileInfo.exists() && fileInfo.isFile())
    {   // If the file exists, add it to the model
        QString fileSize = QString::number((fileInfo.size()/1024)/1024);
        QString fileName = fileInfo.fileName();

        auto fileItem = new QStandardItem{fileName};
        fileItem->setDragEnabled(false);
        fileItem->setDropEnabled(false);
        fileItem->setEditable(false);
        fileItem->setSelectable(false);
        fileItem->setToolTip(fileName);
        QVariant fileVariant = QVariant::fromValue(fileName);
        fileItem->setData(fileVariant);

        auto pathItem = new QStandardItem{filePath};
        pathItem->setDragEnabled(false);
        pathItem->setDropEnabled(false);
        pathItem->setEditable(false);
        pathItem->setSelectable(false);
        pathItem->setToolTip(filePath);
        QVariant pathVariant = QVariant::fromValue(filePath);
        pathItem->setData(pathVariant);

        auto sizeItem = new QStandardItem{fileSize};
        sizeItem->setDragEnabled(false);
        sizeItem->setDropEnabled(false);
        sizeItem->setEditable(false);
        sizeItem->setSelectable(false);
        sizeItem->setToolTip(QString::number(fileInfo.size())+" bytes");
        QVariant sizeVariant = QVariant::fromValue(fileSize);
        sizeItem->setData(sizeVariant);

        auto progressItem = new QStandardItem{};
        progressItem->setDragEnabled(false);
        progressItem->setDropEnabled(false);
        progressItem->setEditable(false);
        progressItem->setSelectable(false);

        auto closeFileItem = new QStandardItem{};
        closeFileItem->setDragEnabled(false);
        closeFileItem->setDropEnabled(false);
        closeFileItem->setEditable(false);
        closeFileItem->setSelectable(false);

        const QList<QStandardItem*> items = {fileItem, pathItem, sizeItem, progressItem, closeFileItem};

        // Search to see if this item is already in the model
        auto addNewItem = true;

        const auto rowCryptoCount = fileListModelCrypto->rowCount();
        const auto rowHashCount = fileListModelHash->rowCount();

        if (cryptoOrHash=="crypto")
        {
            for (auto row = 0; row < rowCryptoCount; ++row)
            {
                auto testItem = fileListModelCrypto->item(row, 1);

                if (testItem->data().toString() == pathItem->data().toString())
                {
                    addNewItem = false;
                }
            }

            if (addNewItem)
            {   // Add the item to the model if it's new
                fileListModelCrypto->appendRow(items);
            }
        }
        else
        {
            for (auto row = 0; row < rowHashCount; ++row)
            {
                auto testItem = fileListModelHash->item(row, 1);

                if (testItem->data().toString() == pathItem->data().toString())
                {
                    addNewItem = false;
                }
            }

            if (addNewItem)
            {   // Add the item to the model if it's new
                fileListModelHash->appendRow(items);
            }
        }


    } // End if file exists and is a file
}

QStringList MainWindow::getListFiles(QString cryptoOrHash)
{
    const int rowCountCrypto = fileListModelCrypto->rowCount();
    const int rowCountHash = fileListModelHash->rowCount();

    QStringList fileList;

    if (cryptoOrHash=="crypto")
    {
        if (0 < rowCountCrypto)
        {
            for (int row = 0; row < rowCountCrypto; ++row)
            {
                QStandardItem* item = fileListModelCrypto->item(row, 1);
                fileList.append(item->text());
            }
        }
    }
    else
    {
        if (0 < rowCountHash)
        {
            for (int row = 0; row < rowCountHash; ++row)
            {
                QStandardItem* item = fileListModelHash->item(row, 1);
                fileList.append(item->text());
            }
        }
    }

    return fileList;
}

void MainWindow::on_pushStartHash_clicked()
{
    if (getListFiles("hash").isEmpty())
    {
        QMessageBox::warning(this, tr("Oups..."),
                             tr("You must add one are more files to start job."));
        return;
    }
    QString algo= ui->comboHash->currentText();

    Hash->setParam(getListFiles("hash"),m_prefs.algoHash);

    Hash->start();
}

void MainWindow::on_pushStartJob_clicked()
{
    bool direction;
    qint32 directionInt = ui->comboDirection->currentIndex();
    if (directionInt==0)
    {
        direction=true;
    }
    else
    {
        direction=false;
    }

    if (getListFiles("crypto").isEmpty())
    {
        QMessageBox::warning(this, tr("Oups..."),
                             tr("You must add one are more files to start job."));
        return;

    }

    if (ui->linePassEncrypt->text().isEmpty() && ui->linePassConfirm->text().isEmpty())
    {
        QMessageBox::warning(this, tr("Oups..."),
                             tr("You must enter the passphrase and the confirmation."));
        return;
    }

    if (ui->linePassEncrypt->text()!=ui->linePassConfirm->text() && direction==true)
    {
        QMessageBox::warning(this, tr("Oups..."),
                             tr("Passphrase and confirmation not match."));
        return;
    }

    Crypto->setParam(getListFiles("crypto"),
                     ui->linePassEncrypt->text(),
                     m_prefs.algoCrypto,
                     direction,
                     m_prefs.algoCrypto,
                     m_prefs.deleteOriginals,
                     ARs::APP_VERSION);

    Crypto->start();
}

void MainWindow::on_comboDirection_currentIndexChanged(int index)
{
    if (index==1)
    {
        ui->linePassConfirm->hide();
        ui->label_Confirm->hide();
    }
    else
    {
        ui->linePassConfirm->show();
        ui->label_Confirm->show();
    }
    m_prefs.cipherDirection = index;
}

void MainWindow::on_pushRemoveAllCrypto_clicked()
{
    while (fileListModelCrypto->rowCount() > 0)
    {
        fileListModelCrypto->removeRow(0);
    }
}

void MainWindow::on_pushRemoveAllHash_clicked()
{
    while (fileListModelHash->rowCount() > 0)
    {
        fileListModelHash->removeRow(0);
    }
}

void MainWindow::on_pushClearLog_clicked()
{
    ui->plainTextLog->clear();
    QFile::remove("arsenic.log.html");
}

void MainWindow::on_actionConfiguration_triggered()
{
    Config::getPreferences(m_prefs, this);
}

void MainWindow::on_actionQuitter_triggered()
{
    this->close();
}

void MainWindow::on_actionA_propos_de_Arsenic_triggered()
{
    About::setText(tr("file:about_files/en/en_about_arsenic.html"), this);
}

void MainWindow::on_actionA_propos_de_Botan_triggered()
{
    About::setText(tr("file:about_files/en/en_about_botan.html"), this);
}

void MainWindow::on_actionA_Propos_de_Argon2_triggered()
{
    About::setText(tr("file:about_files/en/en_about_argon2.html"), this);
}

void MainWindow::on_actionA_Propos_de_Qt_triggered()
{
    QMessageBox::aboutQt(this,tr("About Qt"));
}

void MainWindow::on_comboHash_currentIndexChanged(const QString &arg1)
{
    m_prefs.algoHash = arg1;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Crypto->cancel=true;
    Crypto->wait();
    saveSettings();
    event->accept();
}
