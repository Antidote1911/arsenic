#include "mainwindow.h"
#include "encryptbar.h"
#include "encryptthread.h"
#include "filesystemmodel.h"
#include "divers.h"

using namespace AbstractBarDialogPublic;
using namespace EncryptBarThreadPublic;
using namespace MessagesPublic;



/*******************************************************************************



*******************************************************************************/


EncryptBar::EncryptBar(QWidget *parent, FileSystemModel *arg_ptr_model, const QString
    &arg_password,const QString &arg_userName, QString ext,QString cryptoAlgo,int argonMemory,int argonOps,bool delete_success, const QString *encrypt_name) : AbstractBarDialog(parent),
	ptr_model(arg_ptr_model)
{
    setWindowTitle(tr("Encrypting..."));

	// the message displayed when the user wants to cancel the operation
    ptr_stop_msg->setWindowTitle(tr("Stop execution?"));
    ptr_stop_msg->setText(tr("Are you sure you want to stop encrypting?"));

	// create the appropriate thread
    worker_thread = new EncryptThread(ptr_model, arg_password,arg_userName ,ext,cryptoAlgo,argonMemory,argonOps,delete_success, encrypt_name);
	ptr_model->moveToThread(worker_thread);
    //connect(worker_thread, &MyEncryptThread::updateProgress, this, &updateProgress);
    connect(worker_thread, &EncryptThread::updateProgress, this, &AbstractBarDialog::updateProgress);
    connect(worker_thread, &EncryptThread::updateGeneralProgress, this, &AbstractBarDialog::updateGeneralProgress);
    connect(worker_thread, &EncryptThread::updateStatusText, this, &AbstractBarDialog::updateStatusText);

    m_ui->label->setText(tr("Encrypting list..."));

	worker_thread->start();
}


/*******************************************************************************



*******************************************************************************/


EncryptBar::~EncryptBar()
{
	deleteProgressThread();
}


/*******************************************************************************



*******************************************************************************/


void EncryptBar::handleError(int error)
{
	// the user interrupted the encryption progress, give him a list of what was done

        QMessageBox *error_msg = new QMessageBox(static_cast<MainWindow *>(this->parent()));
	error_msg->setAttribute(Qt::WA_DeleteOnClose);
    error_msg->setWindowTitle(tr("Encryption interrupted!"));
	error_msg->setIcon(QMessageBox::Warning);
	error_msg->setStandardButtons(QMessageBox::Close);
    error_msg->setText(tr("The encryption process was interrupted. Some files or directories might not "
        "have been finished. Click below to show details."));

	// wait for the worker thread to completely finish
	worker_thread->wait();

	QString detailed_text = createDetailedText();

	if(detailed_text == QString())
        detailed_text = tr("No existing items were found!");

	error_msg->setDetailedText(detailed_text);
	error_msg->show();

	ptr_model->startAllDirThread();
}


/*******************************************************************************



*******************************************************************************/


QString EncryptBar::createDetailedText()
{
	QString ret_string;

	// get the encryption status of each item from the worker thread
	std::vector<int> status_list = worker_thread->getStatus();
        const std::vector<FileInfoPtr> &item_list = worker_thread->getItemList();

	// go through each one and list the result right after the full path of the item
    for(unsigned int i = 0; i < item_list.size(); i++)
	{
		ret_string += item_list[i]->getFullPath();
		ret_string += "\n" + errorCodeToString(status_list[i]);

		if(i < status_list.size() - 1)
			ret_string += "\n\n";
	}

	return ret_string;
}


/*******************************************************************************



*******************************************************************************/


void EncryptBar::handleFinished()
{
	// the encryption process was finished, create a list of what was successful and wasn't

        QMessageBox *error_msg = new QMessageBox(static_cast<MainWindow *>(this->parent()));
	error_msg->setAttribute(Qt::WA_DeleteOnClose);
    error_msg->setWindowTitle(tr("Encryption finished!"));
	error_msg->setIcon(QMessageBox::Warning);
	error_msg->setStandardButtons(QMessageBox::Close);
    error_msg->setText(tr("The encryption process was finished. Click below to show details."));

	// wait for the worker thread to completely finished
	worker_thread->wait();

	QString detailed_text = createDetailedText();

	if(detailed_text == QString())
        detailed_text = tr("No existing items were found!");

	error_msg->setDetailedText(detailed_text);
	error_msg->show();

	ptr_model->startAllDirThread();
}


/*******************************************************************************



*******************************************************************************/


void EncryptBar::deleteProgressThread()
{
	if(worker_thread->isRunning())
		worker_thread->requestInterruption();

	worker_thread->wait();
	delete worker_thread;
}


/*******************************************************************************



*******************************************************************************/

void EncryptBar::handleRejectYes()
{
	worker_thread->requestInterruption();
	worker_thread->wait();
}
