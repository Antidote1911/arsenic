#ifndef ENCRYPTBAR_H
#define ENCRYPTBAR_H

#include "abstractbarDialog.h"
#include "messages.h"

// forward declarations
class FileSystemModel;
namespace EncryptBarThreadPublic
{
    class EncryptThread;
}





/*******************************************************************************



*******************************************************************************/


class EncryptBar : public AbstractBarDialog
{
	Q_OBJECT

public:
    EncryptBar(QWidget *parent, FileSystemModel *arg_ptr_model, const QString &arg_password,const QString &arg_userName, QString ext ,QString cryptoAlgo,int argonMemory,int argonOps,bool
		delete_success, const QString *encrypt_name = nullptr);
    ~EncryptBar();

protected:
	void handleFinished() Q_DECL_OVERRIDE;
	void handleError(int error) Q_DECL_OVERRIDE;
	void handleRejectYes() Q_DECL_OVERRIDE;

	void deleteProgressThread();

private:
	QString createDetailedText();
	void cleanModel();
    EncryptBarThreadPublic::EncryptThread *worker_thread;
    FileSystemModel *ptr_model;
};

#endif // ENCRYPTBAR_H
