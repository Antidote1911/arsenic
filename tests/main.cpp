#include <QCoreApplication>
#include <iostream>

#include "../arsenic/crypto/argon2/argon2.h"
#include "../arsenic/crypto/botan/botan_all.h"


using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    uint32_t outlen=24;
    vector<uint8_t> output(outlen);

    string password="password";
    string salt="somesalt";

    uint32_t tcost= 2;
    uint32_t mcost= 1<<16;
    uint32_t paralel=4;

    cout<<"password : "+password<<endl;
    cout<<"salt     : "+salt<<endl;

    auto ret = argon2_hash(tcost,
                           mcost,
                           paralel,
                           password.data(),
                           password.size(),
                           salt.data(),
                           salt.size(),
                           output.data(), outlen, NULL, 0, Argon2_i, ARGON2_VERSION_NUMBER);

    if (ret != ARGON2_OK)
    {
        cerr << "ARGON2 ERROR : " << argon2_error_message(ret) << endl;
    }

    vector<char> encoded(256);

    auto ret2 = argon2i_hash_encoded(tcost, mcost,
                                     paralel, password.data(),
                                     password.size(), salt.data(),
                                     salt.size(), outlen,
                                     encoded.data(), 256);

    if (ret2 != ARGON2_OK)
    {
        cerr << "ARGON2 ERROR : " << argon2_error_message(ret2) << endl;
    }
    //Botan::hex_encode(output,output.size())
    cout<<Botan::hex_encode(output,output.size())<<endl;

    cout<<encoded.data()<<endl;

    return a.exec();
}
