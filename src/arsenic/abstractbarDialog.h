#ifndef ABSTRACTBARDIALOG_H
#define ABSTRACTBARDIALOG_H

#include <QMessageBox>
#include <QDialog>
#include <memory>

#include "ui_abstractbarDialog.h"

namespace Ui {
class AbstractBarDialog;
}


/*******************************************************************************



*******************************************************************************/


namespace AbstractBarDialogPublic
{
	// used for signaling between the class MyAbstractBar and the underlying worker
	enum PROGRESS_UPDATE: int {INTERRUPTED = -3, RESET = -2, FINISHED = -1, MIN = 0,
		MAX = std::numeric_limits<int>::max()};
	enum EXEC_RESULT: int {EXEC_SUCCESS, EXEC_FAILURE};
}


/*******************************************************************************



*******************************************************************************/


class AbstractBarDialog : public QDialog
{
	Q_OBJECT

public:
    AbstractBarDialog(QWidget *parent);
    ~AbstractBarDialog() override;

protected:
	virtual void handleFinished() = 0;
    virtual void handleError(int error) = 0;
	virtual void handleRejectYes() = 0;

    const std::unique_ptr<Ui::AbstractBarDialog> m_ui;
	QMessageBox *ptr_stop_msg;

public slots:
	void updateProgress(int curr);
    void updateGeneralProgress(int val);
    void updateStatusText(QString txt);

private slots:
	void on_cancel_clicked();
	void reject() Q_DECL_OVERRIDE;

private:

};

#endif // ABSTRACTBARDIALOG_H
