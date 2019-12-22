#ifndef PASSGENERATOR_H
#define PASSGENERATOR_H

#include <QWidget>
#include <QDialog>


namespace Ui {
class PassGenerator;
}

class PassGenerator : public QDialog
{
    Q_OBJECT

public:
    explicit PassGenerator(QWidget *parent = nullptr);
    ~PassGenerator();
    QString getPassword();
        QString pass;

private:
    Ui::PassGenerator *ui;


private slots:
    void    generatePassword    ();
    void    lengthChanged       (int length);
    void    pbCopy              ();
};

#endif // PASSGENERATOR_H
