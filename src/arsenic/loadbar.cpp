#include "mainwindow.h"
#include "loadbar.h"
#include "loadthread.h"
#include "filesystemmodel.h"

using namespace AbstractBarDialogPublic;
using namespace LoadBarThreadPublic;
using namespace LoadBarPublic;

/*******************************************************************************

*******************************************************************************/

LoadBar::LoadBar(QWidget *parent, FileSystemModel *arg_ptr_model, QString arg_list_path) :
        AbstractBarDialog(parent), ptr_model(arg_ptr_model), list_path(arg_list_path)
{
	setWindowTitle("Loading...");

	// the message displayed when the user wants to cancel the operation
	ptr_stop_msg->setWindowTitle("Stop execution?");
	ptr_stop_msg->setText("Are you sure you want to stop loading the list?");

	// create the appropriate thread
        worker_thread = new LoadThread(ptr_model, list_path);
	ptr_model->moveToThread(worker_thread);
    //connect(worker_thread, &MyLoadThread::updateProgress, this, &updateProgress);
    connect(worker_thread, &LoadThread::updateProgress, this, &AbstractBarDialog::updateProgress);

        m_ui->label->setText("Loading list...");

	worker_thread->start();
}

/*******************************************************************************

*******************************************************************************/

LoadBar::~LoadBar()
{
	deleteProgressThread();
}

/*******************************************************************************

*******************************************************************************/

void LoadBar::handleError(int error)
{
	if(error != INTERRUPTED)
	{
		// there was a problem and the thread has returned, give the user a warning message. it will
		// automatically delete itself when the user closes it

                QMessageBox *error_msg = new QMessageBox(static_cast<MainWindow *>(this->parent()));
		error_msg->setAttribute(Qt::WA_DeleteOnClose);
		error_msg->setWindowTitle("Error opening list!");
		error_msg->setIcon(QMessageBox::Warning);
		error_msg->setStandardButtons(QMessageBox::Ok);

		switch(error)
		{
			case FILE_READ_ERROR:
				error_msg->setText("There was an error opening the data file.");
				error_msg->show();
				break;

			case STREAM_READ_ERROR:
				error_msg->setText("There was an error reading the list of files and directories.");
				error_msg->show();
				break;
		}
	}
}

/*******************************************************************************

*******************************************************************************/

void LoadBar::handleFinished()
{
	// start the directory size calculation threads for the model
	ptr_model->startAllDirThread();
}

/*******************************************************************************

*******************************************************************************/

void LoadBar::deleteProgressThread()
{
	if(worker_thread->isRunning())
		worker_thread->requestInterruption();

	worker_thread->wait();
	delete worker_thread;
}

/*******************************************************************************

*******************************************************************************/

void LoadBar::handleRejectYes()
{
	worker_thread->requestInterruption();
	worker_thread->wait();
}

/*******************************************************************************

*******************************************************************************/
