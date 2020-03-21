#ifndef LOADBAR_H
#define LOADBAR_H

#include "abstractbarDialog.h"
#include <memory>

// forward declarations
class FileSystemModel;
namespace LoadBarThreadPublic
{
    class LoadThread;
}

namespace LoadBarPublic
{
	// more specific updateProgress() stuff
	enum LOAD_UPDATE: int {FILE_READ_ERROR = std::numeric_limits<int>::min(), STREAM_READ_ERROR};
}


/*******************************************************************************



*******************************************************************************/


class LoadBar : public AbstractBarDialog
{
	Q_OBJECT

public:
    LoadBar(QWidget *parent, FileSystemModel *arg_ptr_model, QString arg_list_path);
    ~LoadBar();

protected:
	void handleFinished() Q_DECL_OVERRIDE;
	void handleError(int error) Q_DECL_OVERRIDE;
	void handleRejectYes() Q_DECL_OVERRIDE;

	void deleteProgressThread();

private:
    LoadBarThreadPublic::LoadThread *worker_thread;
    FileSystemModel *ptr_model;
	QString list_path;
};

#endif // LOADBAR_H
