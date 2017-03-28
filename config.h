#ifndef CONFIG_H
#define CONFIG_H

#include <QDialog>
#include <QSettings>
#include <preferences.h>

namespace Ui {
class Config;
}

class Config : public QDialog
{
    Q_OBJECT

public:
    explicit Config(QWidget *parent = 0);
    ~Config();
    static bool getPreferences(BBp::Preferences &prefs, QWidget* parent = 0);



private slots:



private:
    Ui::Config *ui;




};

#endif // CONFIG_H
