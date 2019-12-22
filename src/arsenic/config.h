#ifndef CONFIG_H
#define CONFIG_H

#include <QDialog>
#include <QSettings>
#include "preferences.h"



namespace Ui {
class Config;
}

class Config : public QDialog
{
    Q_OBJECT

public:
    explicit Config(QWidget *parent = nullptr);
    ~Config();
    static bool getPreferences(ARs::Preferences &prefs, QWidget* parent = nullptr);


private:
    Ui::Config *ui;
};

#endif // CONFIG_H
