#ifndef DERIVATION_H
#define DERIVATION_H

#include <QObject>
#include "botan_all.h"
#include "constants.h"

class Derivation : public QObject
{
    Q_OBJECT
public:

    explicit Derivation(QObject *parent = nullptr);
    void setPassword(QString pass);
    void setSalt(Botan::SecureVector<quint8> salt);
    void setArgonParam(quint32 memlimit, quint32 iterations);
    Botan::SymmetricKey getkey1();
    Botan::SymmetricKey getkey2();
    Botan::SymmetricKey getkey3();



signals:

private:
    void calculateMastersKeys();
    Botan::SecureVector<char> convertStringToVectorChar(QString qstring);
    QString m_password;
    quint32 m_memlimit;
    quint32 m_iterations;
    Botan::SecureVector<quint8> m_salt;
    Botan::SecureVector<char> m_pass_buffer;
    Botan::SymmetricKey m_key1;
    Botan::SymmetricKey m_key2;
    Botan::SymmetricKey m_key3;
};

#endif // DERIVATION_H
