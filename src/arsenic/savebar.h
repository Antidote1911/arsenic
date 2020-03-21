#ifndef SAVEBAR_H
#define SAVEBAR_H

#include "abstractbarDialog.h"
#include <memory>

// forward declarations
class FileSystemModel;
namespace SaveBarThreadPublic
{
        class SaveThread;
}

namespace SaveBarPublic
{
	// more specific updateProgress() stuff
	enum SAVE_UPDATE: int {FILE_WRITE_ERROR = std::numeric_limits<int>::min(), STREAM_WRITE_ERROR};
}


/*******************************************************************************



*******************************************************************************/


class SaveBar : public AbstractBarDialog
{
	Q_OBJECT

public:
        SaveBar(QWidget *parent, FileSystemModel *arg_ptr_model, QString arg_list_path);
        ~SaveBar();

protected:
	void handleFinished() Q_DECL_OVERRIDE;
	void handleError(int error) Q_DECL_OVERRIDE;
	void handleRejectYes() Q_DECL_OVERRIDE;

	void deleteProgressThread();

private:
        SaveBarThreadPublic::SaveThread *worker_thread;
        FileSystemModel *ptr_model;
        QString list_path;
};

#endif // SAVEBAR_H
