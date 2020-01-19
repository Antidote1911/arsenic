#include "mainwindow.h"
#include "decryptbar.h"
#include "decryptthread.h"
#include "filesystemmodel.h"
#include "divers.h"

using namespace AbstractBarDialogPublic;
using namespace DecryptBarThreadPublic;
using namespace MessagesPublic;

/*******************************************************************************

*******************************************************************************/

DecryptBar::DecryptBar(QWidget *parent, FileSystemModel *arg_ptr_model, const QString
    &arg_password,bool delete_success) : AbstractBarDialog(parent), ptr_model(arg_ptr_model)
{
    setWindowTitle(tr("Decrypting..."));

	// the message displayed when the user wants to cancel the operation
    ptr_stop_msg->setWindowTitle(tr("Stop execution?"));
    ptr_stop_msg->setText(tr("Are you sure you want to stop decrypting?"));

	// create the appropriate thread
    worker_thread = new DecryptThread(ptr_model, arg_password, delete_success);
	ptr_model->moveToThread(worker_thread);
    connect(worker_thread, &DecryptThread::updateProgress, this, &AbstractBarDialog::updateProgress);
    connect(worker_thread, &DecryptThread::updateGeneralProgress, this, &AbstractBarDialog::updateGeneralProgress);
    connect(worker_thread, &DecryptThread::updateStatusText, this, &AbstractBarDialog::updateStatusText);


    m_ui->label->setText(tr("Decrypting list..."));

	worker_thread->start();
}

/*******************************************************************************

*******************************************************************************/

DecryptBar::~DecryptBar()
{
	deleteProgressThread();
}

/*******************************************************************************

*******************************************************************************/

void DecryptBar::handleError(int error)
{
	// the user interrupted the decryption progress, give him a list of what was done

        QMessageBox *error_msg = new QMessageBox(static_cast<MainWindow *>(this->parent()));
	error_msg->setAttribute(Qt::WA_DeleteOnClose);
    error_msg->setWindowTitle(tr("Decryption interrupted!"));
	error_msg->setIcon(QMessageBox::Warning);
	error_msg->setStandardButtons(QMessageBox::Close);
    error_msg->setText(tr("The decryption process was interrupted. Some files or directories might not "
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

QString DecryptBar::createDetailedText()
{
    QString ret_string;

    // get the decryption status of each item from the worker thread
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

void DecryptBar::handleFinished()
{
	// the decryption process was finished, create a list of what was successful and wasn't

        QMessageBox *error_msg = new QMessageBox(static_cast<MainWindow *>(this->parent()));
	error_msg->setAttribute(Qt::WA_DeleteOnClose);
    error_msg->setWindowTitle(tr("Decryption finished!"));
	error_msg->setIcon(QMessageBox::Warning);
	error_msg->setStandardButtons(QMessageBox::Close);
    error_msg->setText(tr("The decryption process was finished. Click below to show details."));

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

void DecryptBar::deleteProgressThread()
{
	if(worker_thread->isRunning())
		worker_thread->requestInterruption();

	worker_thread->wait();
	delete worker_thread;
}

/*******************************************************************************

*******************************************************************************/

void DecryptBar::handleRejectYes()
{
	worker_thread->requestInterruption();
	worker_thread->wait();
}

/*******************************************************************************

*******************************************************************************/
