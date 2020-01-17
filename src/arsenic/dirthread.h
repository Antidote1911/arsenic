#ifndef DIRTHREAD_H
#define DIRTHREAD_H

#include <climits>
#include <QThread>

class FileSystemModel;
class QFile;

/*******************************************************************************



*******************************************************************************/


namespace FileInfoPublicThread
{


	/*******************************************************************************



	*******************************************************************************/


    class DirThread : public QThread
	{
		Q_OBJECT

	public:
		//MyDirThread() {}
        DirThread(const QString &arg_path);

	protected:
		void run() Q_DECL_OVERRIDE;
		//void setDirPath(const QString &arg_path) {dir_path = arg_path;}
		//QString getDirPath() const {return dir_path;}

	signals:
		void updateSize(qint64 info);

	private:
		const QString dir_path;
		qint64 getDirSize(const QString &curr_path) const;
	};
}

#endif // DIRTHREAD_H
