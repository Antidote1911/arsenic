#include "passgenerator.h"
#include "ui_passgenerator.h"
#include <QCheckBox>
#include <QTime>
#include <QSpinBox>
#include <QSlider>
#include <iostream>
#include "botan_all.h"


static const QString digitsS = "0123456789";
static const QString lettersS = "abcdefghijklmnopqrstuvwxyz";
static const QString LETTERSS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const QString symbolsS = "!@#$%^&*()-=_+{}[]\\|/?.,';:`";

//======================================= Конструктор
PassGenerator::PassGenerator(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PassGenerator)
{
    ui->setupUi(this);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &PassGenerator::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &PassGenerator::reject);

    connect(ui->cbDigit,    &QCheckBox      ::clicked,
            this,           &PassGenerator  ::generatePassword);
    connect(ui->cbLETTERS,  &QCheckBox      ::clicked,
            this,           &PassGenerator  ::generatePassword);
    connect(ui->cbLetters,  &QCheckBox      ::clicked,
            this,           &PassGenerator  ::generatePassword);
    connect(ui->cbSymbols,  &QCheckBox      ::clicked,
            this,           &PassGenerator  ::generatePassword);
    connect(ui->sbLength,   SIGNAL(valueChanged(int)),
            this,           SLOT(lengthChanged(int)));
    connect(ui->slLength,   &QSlider        ::valueChanged,
            this,           &PassGenerator  ::lengthChanged );
    connect(ui->pbGenerate, &QPushButton    ::pressed,
            this,           &PassGenerator  ::generatePassword);
    connect(ui->pbCopy,     &QPushButton    ::pressed,
            this,           &PassGenerator  ::pbCopy    );

    generatePassword();
}

//======================================= Деструктор
PassGenerator::~PassGenerator() {delete ui;}



//======================================= Функция генерирования нового пароля
void PassGenerator::generatePassword()
{
    QString alphabet {""};

    if (ui->cbDigit->isChecked())   alphabet.append(digitsS);
    if (ui->cbLetters->isChecked()) alphabet.append(lettersS);
    if (ui->cbLETTERS->isChecked()) alphabet.append(LETTERSS);
    if (ui->cbSymbols->isChecked()) alphabet.append(symbolsS);

    if (alphabet == "")
    {
        ui->lePassword->setText("");
        return;
    }

    QString password {""};

    for (auto i {ui->sbLength->value()}, alphabetSize {alphabet.size()}; --i >= 0;)
        password.append(alphabet.at(Botan::Sodium::randombytes_uniform(alphabetSize) ));

    ui->lePassword->setText(password);
}




//======================================= Изменение длины пароля
void PassGenerator::lengthChanged(int length)
{
    (ui->slLength->value() == length) ? ui->sbLength->setValue(length) : ui->slLength->setValue(length);
    generatePassword();
}




//======================================= Кнопка копирования в память
void PassGenerator::pbCopy()
{
    ui->lePassword->selectAll();
    ui->lePassword->copy();
}


QString PassGenerator::getPassword()
{    
    return ui->lePassword->text();
}
