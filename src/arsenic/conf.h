#ifndef CONF_H
#define CONF_H

#include <QDialog>
#include <QSettings>



namespace Ui {
class Conf;
}

class Conf : public QDialog
{
    Q_OBJECT

public:
    explicit Conf(QWidget *parent = nullptr);
    ~Conf();
    void loadSettings();

private slots:
    void saveSettings();
    //void reject();



private:
    Ui::Conf *ui;

};

#endif // CONF_H
