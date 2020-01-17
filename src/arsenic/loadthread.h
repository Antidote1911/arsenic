#ifndef LOADTHREAD_H
#define LOADTHREAD_H

#include <QThread>

class FileSystemModel;

namespace LoadBarThreadPublic
{


	/*******************************************************************************



	*******************************************************************************/


    class LoadThread : public QThread
	{
		Q_OBJECT

	public:
        LoadThread(FileSystemModel *arg_model, const QString &arg_path);
		int getStatus() const;

	protected:
		void run() Q_DECL_OVERRIDE;

	signals:
		void updateProgress(int curr);

	private:
		void interruptionPoint();
		void runHelper();

        FileSystemModel *ptr_model;
		const QString list_path;
		int status;
	};
}

#endif // LOADTHREAD_H
