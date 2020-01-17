#include <QTextStream>

#include "dirthread.h"
#include "filesystemmodel.h"

// trivial exception class used for interrupting thread execution in class MyDirThread
#include "interrupt.h"

using namespace FileInfoPublicThread;
using namespace FileSystemModelPublic;


/*******************************************************************************



*******************************************************************************/


DirThread::DirThread(const QString &arg_path) : dir_path(arg_path) {}


/*******************************************************************************



*******************************************************************************/


void DirThread::run()
{
	qint64 size = 0;

	// catch any exceptions caused by a requestInterruption() from another thread and stop if so
	try
	{
		// recursive function to parse through directories
		size = getDirSize(dir_path);
	}
        catch(Interrupt &e)
	{
		return;
	}

	// finished, signal the appropriate value and done
	emit updateSize(size);
}


/*******************************************************************************



*******************************************************************************/


qint64 DirThread::getDirSize(const QString &curr_dir) const
{
	qint64 size = 0;

	// get a list of elements in the current directory

	QDir qdir = QDir(curr_dir);
	QList<QFileInfo> list = qdir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoSymLinks |
		QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

	for(int i = 0; i < list.size(); i++)
	{
		QFileInfo file = list.at(i);

		// check if an interruption was requested
		if(QThread::currentThread()->isInterruptionRequested())
		{
                        throw Interrupt();
		}

		// otherwise, continue parsing files and directories
		if(file.isFile())
			size += file.size();
		else if(file.isDir())
			size += getDirSize(file.absoluteFilePath());
	}

	return size;
}
