#include "argonTests.h"
#include "ui_argonTests.h"

ArgonTests::ArgonTests(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::ArgonTests)
{
    m_ui->setupUi(this);
}

ArgonTests::~ArgonTests() {}
