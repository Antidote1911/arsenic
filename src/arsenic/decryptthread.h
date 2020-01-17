#ifndef DECRYPTTHREAD_H
#define DECRYPTTHREAD_H

#include "fileinfo.h"

#include <QThread>

class FileSystemModel;

namespace DecryptBarThreadPublic
{



	/*******************************************************************************



	*******************************************************************************/


    class DecryptThread : public QThread
	{
		Q_OBJECT

	public:
        DecryptThread(FileSystemModel *arg_model,const QString &arg_password, bool
			arg_delete_success);
		std::vector<int> getStatus() const;
        const std::vector<FileInfoPtr> &getItemList() const;


	protected:
		void run() Q_DECL_OVERRIDE;

	signals:
		void updateProgress(int curr);
        void updateGeneralProgress(int val);
        void updateStatusText(QString txt);

	private:
		void interruptionPoint();
		void runHelper();

        FileSystemModel *ptr_model;
		std::vector<int> status_list;
        std::vector<FileInfoPtr> item_list;
        const std::vector<FileInfoPtr> empty_list;
		QString password;
		bool delete_success;
        int myDecryptFile(const QString &des_path, const QString &src_path, const QString &key, QString
            *decrypt_name = nullptr);

	};
}

#endif // DECRYPTTHREAD_H
