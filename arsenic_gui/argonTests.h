#pragma once

#include <QDialog>
#include <memory>

namespace Ui {
class ArgonTests;
}

class ArgonTests : public QDialog {
    Q_OBJECT

  public:
    explicit ArgonTests(QWidget* parent = nullptr);
    ~ArgonTests();

  private:
    const std::unique_ptr<Ui::ArgonTests> m_ui;
};
