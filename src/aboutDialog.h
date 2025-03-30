#pragma once

#include <QDialog>
#include <memory>

#include "arscore/consts.h"


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
    const std::unique_ptr<Ui::AboutDialog> m_ui;
    const std::unique_ptr<consts> m_const;
};
