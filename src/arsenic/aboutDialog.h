#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog {
    Q_OBJECT

  public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

  protected slots:
    void copyToClipboard();

  private:
    const QScopedPointer<Ui::AboutDialog> m_ui;
};

#endif // ABOUTDIALOG_H
