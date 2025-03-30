#pragma once

#include <QDialog>
#include <QMessageBox>
#include <memory>

namespace Ui {
class HashCheckDialog;
}

class HashCheckDialog : public QDialog {
    Q_OBJECT

  public:
    explicit HashCheckDialog(QWidget *parent = nullptr);
    ~HashCheckDialog() Q_DECL_OVERRIDE;

  protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

  private slots:
    void openFile();
    void calculate(const QString &text);
    void cancel();
    void messageBox(QMessageBox::Icon icon, const QString &title, const QString &message);
    void textChanged(const QString &text);
    void copyToClipboard();

  private:
    const std::unique_ptr<Ui::HashCheckDialog> m_ui;
    bool cancel_calculation;
    bool isCalculating;
};
