#ifndef MYENCRYPTTHREAD_H
#define MYENCRYPTTHREAD_H

#include "myfileinfo.h"
#include <QThread>

class MyFileSystemModel;

namespace MyEncryptBarThreadPublic
{



	/*******************************************************************************



	*******************************************************************************/


	class MyEncryptThread : public QThread
	{
		Q_OBJECT

	public:
        MyEncryptThread(MyFileSystemModel *arg_model, const QString &arg_password,const QString &arg_userName,QString ext,QString cryptoAlgo,int argonmem,int argonops,bool
            arg_delete_success, const QString *arg_encrypt_name);
		std::vector<int> getStatus() const;
		const std::vector<MyFileInfoPtr> &getItemList() const;



	protected:
		void run() Q_DECL_OVERRIDE;

	signals:
        void updateProgress(int curr);
        void updateGeneralProgress(qint64 val);
        void updateStatusText(QString txt);

	private:
		void interruptionPoint();
		void runHelper();
		int getNextFilenameSuffix(const QString &path, const QString &base_name, const QString
			&file_type, int start_num = 0,	int	max_tries = 1000) const;

		MyFileSystemModel *ptr_model;
		std::vector<int> status_list;
		std::vector<MyFileInfoPtr> item_list;
		const std::vector<MyFileInfoPtr> empty_list;
		QString password;
        QString userName;
        QString ext;
        QString cryptoAlgo;
        int argonmem;
        int argonops;        
		bool delete_success;
        const QString *encrypt_name;
        int myEncryptFile(const QString &des_path, const QString &src_path, const QString &key,const QString &userName ,int argonmem,int argonops,QString cryptoAlgo);

	};
}

#endif // MYENCRYPTTHREAD_H
