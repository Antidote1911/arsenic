#include <QEventLoop>
#include <QPlainTextEdit>

#include "abstractbarDialog.h"
#include "ui_abstractbarDialog.h"
#include "filesystemmodel.h"


using namespace AbstractBarDialogPublic;

/*******************************************************************************

*******************************************************************************/

AbstractBarDialog::AbstractBarDialog(QWidget *parent) : QDialog(parent), m_ui(new Ui::AbstractBarDialog),
	ptr_stop_msg(new QMessageBox(this))
{
	// setup the basic progress bar and message box layout

    m_ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_ui->progress_bar->setRange(MIN, MAX);
    m_ui->progress_bar->setValue(MIN);

	ptr_stop_msg->setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	ptr_stop_msg->setIcon(QMessageBox::Question);
}

/*******************************************************************************

*******************************************************************************/

AbstractBarDialog::~AbstractBarDialog()
{
	delete ptr_stop_msg;
}

/*******************************************************************************

*******************************************************************************/

void AbstractBarDialog::updateProgress(int curr)
{
	// check if there was an error
	if(curr < RESET)
	{
		// there was a problem, handle it and close the progress bar
		handleError(curr);

		// destroy the progress bar afterwards
		if(!ptr_stop_msg->isHidden())
			ptr_stop_msg->done(QMessageBox::Cancel);

		// quit in failure
		this->done(EXEC_FAILURE);
	}

	// check if the thread is finished
	else if(curr == FINISHED)
	{
		handleFinished();

		// close the on_cancel_clicked() message box if open
		if(!ptr_stop_msg->isHidden())
			ptr_stop_msg->done(QMessageBox::Cancel);

		// quit successfully
		this->done(EXEC_SUCCESS);
	}

	// otherwise, the signal was an update to the progress bar
	else
        m_ui->progress_bar->setValue(curr);
}

/*******************************************************************************

*******************************************************************************/

void AbstractBarDialog::updateGeneralProgress(int val)
{
    m_ui->progress_Bar_current->setValue(val);
}

/*******************************************************************************

*******************************************************************************/

void AbstractBarDialog::updateStatusText(QString text)
{
    m_ui->plainTextProgress->appendPlainText(text);
}

/*******************************************************************************

*******************************************************************************/

void AbstractBarDialog::on_cancel_clicked()
{
	// reject() is called for close events and the esc key as well
	reject();
}

/*******************************************************************************

*******************************************************************************/

void AbstractBarDialog::reject()
{
	// only respond if the progress bar isn't hidden
	if(!this->isHidden())
	{
		if(ptr_stop_msg->exec() == QMessageBox::Yes)
			// user chose to stop
			handleRejectYes();
	}
}

/*******************************************************************************

*******************************************************************************/
