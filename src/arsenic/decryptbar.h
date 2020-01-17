#ifndef DECRYPTBAR_H
#define DECRYPTBAR_H

#include "abstractbarDialog.h"
#include "messages.h"


// forward declarations
class FileSystemModel;
namespace DecryptBarThreadPublic
{
    class DecryptThread;
}





/*******************************************************************************



*******************************************************************************/


class DecryptBar : public AbstractBarDialog
{
	Q_OBJECT

public:
    DecryptBar(QWidget *parent, FileSystemModel *arg_ptr_model, const QString &arg_password,bool
		delete_success);
    ~DecryptBar() override;


protected:
	void handleFinished() Q_DECL_OVERRIDE;
	void handleError(int error) Q_DECL_OVERRIDE;
	void handleRejectYes() Q_DECL_OVERRIDE;

	void deleteProgressThread();

private:
    QString createDetailedText();
	void cleanModel();
    DecryptBarThreadPublic::DecryptThread *worker_thread;
    FileSystemModel *ptr_model;

};

#endif // DECRYPTBAR_H
