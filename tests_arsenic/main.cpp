#include <QCoreApplication>
#include <iostream>
#include "../arsenic/crypto/argonhash.h"


using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    std::cout << "Hello Test Arsenic !" << std::endl;
    return a.exec();
}
