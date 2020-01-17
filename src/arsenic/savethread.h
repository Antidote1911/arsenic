#ifndef SAVETHREAD_H
#define SAVETHREAD_H

#include <QThread>

class FileSystemModel;
class QFile;

namespace SaveBarThreadPublic
{


	/*******************************************************************************



	*******************************************************************************/


    class SaveThread : public QThread
	{
		Q_OBJECT

	public:
        SaveThread(FileSystemModel *arg_model, const QString &arg_path);
		int getStatus() const;

	protected:
		void run() Q_DECL_OVERRIDE;

	signals:
		void updateProgress(int curr);

	private:
		void interruptionPoint(QFile *list_file);
		void runHelper();

        FileSystemModel *ptr_model;
		const QString list_path;
		int status;
	};
}

#endif // SAVETHREAD_H
