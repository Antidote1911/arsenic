#include <QTextStream>
#include <QFile>

#include "savethread.h"
#include "savebar.h"
#include "interrupt.h"
#include "filesystemmodel.h"

using namespace AbstractBarDialogPublic;
using namespace SaveBarThreadPublic;
using namespace SaveBarPublic;
using namespace FileSystemModelPublic;

/*******************************************************************************

*******************************************************************************/

SaveThread::SaveThread(FileSystemModel *arg_model, const QString &arg_path) :
	ptr_model(arg_model), list_path(arg_path), status(RESET) {}


/*******************************************************************************

*******************************************************************************/

int SaveThread::getStatus() const
{
	if(this->isRunning())
		return MIN;
	else
		return status;
}

/*******************************************************************************

*******************************************************************************/

void SaveThread::interruptionPoint(QFile *list_file)
{
	if(QThread::currentThread()->isInterruptionRequested())
	{
		list_file->remove();
		ptr_model->moveToThread(this->thread());
		status = INTERRUPTED;
		emit updateProgress(INTERRUPTED);
                throw Interrupt();
	}
}

/*******************************************************************************

*******************************************************************************/

void SaveThread::run()
{
	try
	{
		runHelper();
	}
        catch(Interrupt &e)
	{
		return;
	}
}

/*******************************************************************************

*******************************************************************************/

void SaveThread::runHelper()
{
	QFile list_file(list_path);

	// create the file which will store the list of files and directories currently in the model
	if(!list_file.open(QIODevice::WriteOnly))
	{
		list_file.remove();
		ptr_model->moveToThread(this->thread());
		status = FILE_WRITE_ERROR;
		emit updateProgress(FILE_WRITE_ERROR);
                throw Interrupt();
	}

	QTextStream list_stream(&list_file);
	list_stream.setCodec("UTF-8");

	// don't save any missing, redundant files or directories
	ptr_model->removeEmpty();
	interruptionPoint(&list_file);
	ptr_model->removeRootDir();
	interruptionPoint(&list_file);
	ptr_model->removeRedundant();
	interruptionPoint(&list_file);
    ptr_model->sorting();

	emit updateProgress(MIN);

	int total_num = ptr_model->rowCount();

	// write the list of paths to the file
	for(int i = 0; i < total_num; i++)
	{
		interruptionPoint(&list_file);

		// get the full path with the filename and insert it into the list file
		QString full_path =
			QDir::cleanPath(ptr_model->data(ptr_model->index(i, FULL_PATH_HEADER)).toString()) + '\n';
		list_stream << full_path;

		// check errors
		if(list_stream.status() == QTextStream::WriteFailed)
		{
			list_file.remove();
			ptr_model->moveToThread(this->thread());
			status = STREAM_WRITE_ERROR;
			emit updateProgress(STREAM_WRITE_ERROR);
                        throw Interrupt();
		}

		// update the progress bar
		emit updateProgress((MAX / total_num) * (i + 1));
	}

	// update the progress bar to 100%
	emit updateProgress(MAX);

	// push the model back to the caller's thread
	ptr_model->moveToThread(this->thread());

	// tells the progress bar to quit successfully
	status = FINISHED;
	emit updateProgress(FINISHED);
}

/*******************************************************************************

*******************************************************************************/
