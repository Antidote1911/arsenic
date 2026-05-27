#pragma once

#include <QDialog>
#include <QSettings>
#include <memory>

namespace Ui {
class ConfigDialog;
}

class ConfigDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ConfigDialog(QWidget* parent = nullptr);
    ~ConfigDialog();
    void loadSettings();

  private slots:
    void saveSettings();
    // void reject();

  private:
    const std::unique_ptr<Ui::ConfigDialog> m_ui;
};
