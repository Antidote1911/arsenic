#ifndef MYDECRYPTTHREAD_H
#define MYDECRYPTTHREAD_H

#include "myfileinfo.h"

#include <QThread>

class MyFileSystemModel;

namespace MyDecryptBarThreadPublic
{



	/*******************************************************************************



	*******************************************************************************/


	class MyDecryptThread : public QThread
	{
		Q_OBJECT

	public:
        MyDecryptThread(MyFileSystemModel *arg_model,const QString &arg_password, bool
			arg_delete_success);
		std::vector<int> getStatus() const;
		const std::vector<MyFileInfoPtr> &getItemList() const;


	protected:
		void run() Q_DECL_OVERRIDE;

	signals:
		void updateProgress(int curr);
        void updateGeneralProgress(int val);
        void updateStatusText(QString txt);

	private:
		void interruptionPoint();
		void runHelper();

		MyFileSystemModel *ptr_model;
		std::vector<int> status_list;
		std::vector<MyFileInfoPtr> item_list;
		const std::vector<MyFileInfoPtr> empty_list;
		QString password;
		bool delete_success;
        int myDecryptFile(const QString &des_path, const QString &src_path, const QString &key, QString
            *decrypt_name = nullptr);

	};
}

#endif // MYDECRYPTTHREAD_H
