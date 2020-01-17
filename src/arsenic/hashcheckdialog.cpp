#include "hashcheckdialog.h"
#include "ui_hashcheckdialog.h"
#include <QCryptographicHash>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QClipboard>
#include <QCloseEvent>
#include <QMimeData>
#include <QLineEdit>
#include <QDebug>
#include <QComboBox>

#include "botan_all.h"
#include "divers.h"

#include "messages.h"
using namespace MessagesPublic;


HashCheckDialog::HashCheckDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HashCheckDialog)
{
    ui->setupUi(this);

    ui->cancelButton->hide();
    ui->progressBar->hide();

    setFixedHeight(sizeHint().height());


    qRegisterMetaType<QMessageBox::Icon>("QMessageBox::Icon");

    connect(ui->open, &QPushButton::clicked, this, &HashCheckDialog::openFile);
    //connect(ui->closeButton, &QPushButton::clicked, this, &HashCheckDialog::close);
    connect(ui->calculateButton, &QPushButton::clicked, this, [=]{ calculate(ui->hashSelector->currentText()); });

    connect(ui->hashSelector, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
        [=](QString text){ calculate(text); });

    connect(ui->copyButton, &QPushButton::clicked, this, &HashCheckDialog::copyToClipboard);
    connect(ui->fileEdit, &QLineEdit::textChanged, this, &HashCheckDialog::textChanged);
    connect(ui->cancelButton, &QPushButton::clicked, this, &HashCheckDialog::cancel);

}

void HashCheckDialog::cancel()
{
    cancel_calculation = true;
}

HashCheckDialog::~HashCheckDialog()
{
           delete ui;
}

void HashCheckDialog::copyToClipboard()
{
    QApplication::clipboard()->setText(ui->checksumEdit->text());
}

void HashCheckDialog::openFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load file..."));
    if(!filename.isEmpty()) {
        ui->fileEdit->setText(filename);
    }
    QFileInfo f_info(filename);
    if(f_info.isFile()) {
        ui->calculateButton->click();
    }

}

void HashCheckDialog::calculate(const QString &text)
{

    ui->progressBar->show();
    ui->cancelButton->show();
    ui->calculateButton->hide();
    ui->checksumEdit->hide();
    ui->open->setDisabled(true);
    ui->checksumEdit->setText("");

    QFile file(ui->fileEdit->text());
    QFileInfo f_info(ui->fileEdit->text());
    if(f_info.isFile())
    {
        file.open(QIODevice::ReadOnly);

        const int block_size = (file.size() > 1024*1024) ? 10*1024 : 1024;
        //char buffer[10*1024]; // allocate enough space for both cases so msvc will be happy
        int bytes_read;

        int progress_max = file.size()/block_size;
        progress_max = (progress_max > 0) ? progress_max : 1;
        ui->progressBar->setMaximum(progress_max);
        ui->progressBar->reset();

        Botan::SecureVector<uint8_t> buf(10*1024);

        //QCryptographicHash hash(hash_algorithm);

        cancel_calculation = false;
        isCalculating = true;
        std::unique_ptr<Botan::HashFunction> hash2(Botan::HashFunction::create(text.toStdString()));


        while( (hash2 && !cancel_calculation && (bytes_read = file.read(reinterpret_cast<char *>(buf.data()), block_size))>0) )
        {
            //hash.addData(buffer, bytes_read);

            hash2->update(buf.data(),bytes_read);
            ui->progressBar->setValue(ui->progressBar->value()+1);
            QCoreApplication::processEvents();
        }

        if (hash2 && !cancel_calculation)
        {
            QString result = QString::fromStdString(Botan::hex_encode(hash2->final()));
            ui->checksumEdit->setText(result);
        }
        if (!hash2)
        {
            ui->checksumEdit->setText("Invalid algo");
        }

    } else
        {
            ui->checksumEdit->setText(errorCodeToString(SRC_CANNOT_OPEN_READ));
        }

    ui->open->setDisabled(false);
    ui->progressBar->hide();
    ui->checksumEdit->show();

    ui->cancelButton->hide();
    ui->calculateButton->show();
    file.close();
    isCalculating = false;

}


void HashCheckDialog::messageBox(QMessageBox::Icon icon, const QString& title, const QString& message)
{
    QMessageBox* box = new QMessageBox(icon, title, message, QMessageBox::Ok, this);
    box->show();
    if(icon == QMessageBox::Critical)
    {
        ui->checksumEdit->setText(tr("error"));
    }
}


void HashCheckDialog::closeEvent(QCloseEvent *event)
{
    if (isCalculating)
    {
        event->ignore();
    }
    else
        event->accept();

}
void HashCheckDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void HashCheckDialog::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;

    QString fileName = urls.first().toLocalFile();
    ui->fileEdit->setText(fileName);
    ui->calculateButton->click();
}

void HashCheckDialog::textChanged(const QString &text)
{
    QFileInfo f_info(text);
    if(f_info.isFile()) {
        ui->calculateButton->setEnabled(true);
    } else {
        ui->calculateButton->setEnabled(false);
        ui->checksumEdit->setText(errorCodeToString(SRC_CANNOT_OPEN_READ));
    }
}
