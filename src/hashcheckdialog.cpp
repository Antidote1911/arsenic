#include "hashcheckdialog.h"
#include "botan_all.h"
#include "ui_hashcheckdialog.h"
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDebug>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>

HashCheckDialog::HashCheckDialog(QWidget *parent)
    : QDialog(parent), m_ui(new Ui::HashCheckDialog)
{
    m_ui->setupUi(this);

    m_ui->cancelButton->hide();
    m_ui->progressBar->hide();

    setFixedHeight(sizeHint().height());

    qRegisterMetaType<QMessageBox::Icon>("QMessageBox::Icon");

    connect(m_ui->open, &QPushButton::clicked, this, &HashCheckDialog::openFile);
    // connect(ui->closeButton, &QPushButton::clicked, this, &HashCheckDialog::close);
    connect(m_ui->calculateButton, &QPushButton::clicked, this, [=, this] { calculate(m_ui->hashSelector->currentText()); });

    connect(m_ui->hashSelector, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged), [=, this](QString text) { calculate(text); });

    connect(m_ui->copyButton, &QPushButton::clicked, this, &HashCheckDialog::copyToClipboard);
    connect(m_ui->fileEdit, &QLineEdit::textChanged, this, &HashCheckDialog::textChanged);
    connect(m_ui->cancelButton, &QPushButton::clicked, this, &HashCheckDialog::cancel);
}

void HashCheckDialog::cancel()
{
    cancel_calculation = true;
}

HashCheckDialog::~HashCheckDialog() {}

void HashCheckDialog::copyToClipboard()
{
    QApplication::clipboard()->setText(m_ui->checksumEdit->text());
}

void HashCheckDialog::openFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load file..."));

    if (!filename.isEmpty()) {
        m_ui->fileEdit->setText(filename);
    }

    QFileInfo f_info(filename);

    if (f_info.isFile()) {
        m_ui->calculateButton->click();
    }
}

void HashCheckDialog::calculate(const QString &text)
{
    m_ui->progressBar->show();
    m_ui->cancelButton->show();
    m_ui->calculateButton->hide();
    m_ui->checksumEdit->hide();
    m_ui->open->setDisabled(true);
    m_ui->checksumEdit->setText("");

    QFile file(m_ui->fileEdit->text());
    QFileInfo f_info(m_ui->fileEdit->text());

    if (f_info.isFile()) {
        file.open(QIODevice::ReadOnly);

        const quint32 block_size = (file.size() > 1024 * 1024) ? 10 * 1024 : 1024;
        // char buffer[10*1024]; // allocate enough space for both cases so msvc will be happy
        quint32 bytes_read;

        quint32 progress_max = file.size() / block_size;
        progress_max         = (progress_max > 0) ? progress_max : 1;
        m_ui->progressBar->setMaximum(progress_max);
        m_ui->progressBar->reset();

        Botan::SecureVector<quint8> buf(10 * 1024);

        // QCryptographicHash hash(hash_algorithm);

        cancel_calculation = false;
        isCalculating      = true;
        std::unique_ptr<Botan::HashFunction> hash2(Botan::HashFunction::create(text.toStdString()));

        while ((hash2 && !cancel_calculation && (bytes_read = file.read(reinterpret_cast<char *>(buf.data()), block_size)) > 0)) {
            // hash.addData(buffer, bytes_read);

            hash2->update(buf.data(), bytes_read);
            m_ui->progressBar->setValue(m_ui->progressBar->value() + 1);
            QCoreApplication::processEvents();
        }

        if (hash2 && !cancel_calculation) {
            QString result = QString::fromStdString(Botan::hex_encode(hash2->final()));
            m_ui->checksumEdit->setText(result);
        }

        if (!hash2) {
            m_ui->checksumEdit->setText("Invalid algo");
        }
    }
    else {
        m_ui->checksumEdit->setText("SRC_CANNOT_OPEN_READ");
    }

    m_ui->open->setDisabled(false);
    m_ui->progressBar->hide();
    m_ui->checksumEdit->show();

    m_ui->cancelButton->hide();
    m_ui->calculateButton->show();
    file.close();
    isCalculating = false;
}

void HashCheckDialog::messageBox(QMessageBox::Icon icon, const QString &title, const QString &message)
{
    QMessageBox *box = new QMessageBox(icon, title, message, QMessageBox::Ok, this);
    box->show();

    if (icon == QMessageBox::Critical) {
        m_ui->checksumEdit->setText(tr("error"));
    }
}

void HashCheckDialog::closeEvent(QCloseEvent *event)
{
    if (isCalculating) {
        event->ignore();
    }
    else {
        event->accept();
    }
}

void HashCheckDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list")) {
        event->acceptProposedAction();
    }
}

void HashCheckDialog::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    if (urls.isEmpty()) {
        return;
    }

    QString fileName = urls.first().toLocalFile();
    m_ui->fileEdit->setText(fileName);
    m_ui->calculateButton->click();
}

void HashCheckDialog::textChanged(const QString &text)
{
    QFileInfo f_info(text);

    if (f_info.isFile()) {
        m_ui->calculateButton->setEnabled(true);
    }
    else {
        m_ui->calculateButton->setEnabled(false);
        m_ui->checksumEdit->setText("SRC_CANNOT_OPEN_READ");
    }
}
