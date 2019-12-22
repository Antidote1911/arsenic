#ifndef ARGONTESTS_H
#define ARGONTESTS_H

#include <QDialog>

namespace Ui {
class Argontests;
}

class Argontests : public QDialog
{
    Q_OBJECT

public:
    explicit Argontests(QWidget *parent = nullptr);
    ~Argontests();

private:
    Ui::Argontests *ui;
};

#endif // ARGONTESTS_H
