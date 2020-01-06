#ifndef MYDECRYPTBAR_H
#define MYDECRYPTBAR_H

#include "myabstractbar.h"
#include "messages.h"


// forward declarations
class MyFileSystemModel;
namespace MyDecryptBarThreadPublic
{
	class MyDecryptThread;
}





/*******************************************************************************



*******************************************************************************/


class MyDecryptBar : public MyAbstractBar
{
	Q_OBJECT

public:
    MyDecryptBar(QWidget *parent, MyFileSystemModel *arg_ptr_model, const QString &arg_password,bool
		delete_success);
    ~MyDecryptBar() override;


protected:
	void handleFinished() Q_DECL_OVERRIDE;
	void handleError(int error) Q_DECL_OVERRIDE;
	void handleRejectYes() Q_DECL_OVERRIDE;

	void deleteProgressThread();

private:
    QString createDetailedText();
	void cleanModel();
	MyDecryptBarThreadPublic::MyDecryptThread *worker_thread;
	MyFileSystemModel *ptr_model;

};

#endif // MYDECRYPTBAR_H
