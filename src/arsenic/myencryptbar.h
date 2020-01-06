#ifndef MYENCRYPTBAR_H
#define MYENCRYPTBAR_H

#include "myabstractbar.h"
#include "messages.h"

// forward declarations
class MyFileSystemModel;
namespace MyEncryptBarThreadPublic
{
	class MyEncryptThread;
}





/*******************************************************************************



*******************************************************************************/


class MyEncryptBar : public MyAbstractBar
{
	Q_OBJECT

public:
    MyEncryptBar(QWidget *parent, MyFileSystemModel *arg_ptr_model, const QString &arg_password,const QString &arg_userName, QString ext ,QString cryptoAlgo,int argonMemory,int argonOps,bool
		delete_success, const QString *encrypt_name = nullptr);
	~MyEncryptBar();

protected:
	void handleFinished() Q_DECL_OVERRIDE;
	void handleError(int error) Q_DECL_OVERRIDE;
	void handleRejectYes() Q_DECL_OVERRIDE;

	void deleteProgressThread();

private:
	QString createDetailedText();
	void cleanModel();
	MyEncryptBarThreadPublic::MyEncryptThread *worker_thread;
	MyFileSystemModel *ptr_model;
};

#endif // MYENCRYPTBAR_H
