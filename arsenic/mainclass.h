#pragma once

#include <QObject>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <memory>
#include "TripleCryptoThread.h"
#include "consts.h"
#include "tqdm.h"

class MainClass : public QObject {
    Q_OBJECT

  private:
    QCoreApplication *app;

    std::unique_ptr<Triple_Crypto_Thread> m_crypto = std::make_unique<Triple_Crypto_Thread>();
    std::unique_ptr<consts> m_const         = std::make_unique<consts>();
    tqdm bar;

  public:
    explicit MainClass(QObject *parent = 0);
    /////////////////////////////////////////////////////////////
    /// Call this to quit application
    /////////////////////////////////////////////////////////////
    void quit();

  signals:
    /////////////////////////////////////////////////////////////
    /// Signal to finish, this is connected to Application Quit
    /////////////////////////////////////////////////////////////
    void finished();

  public slots:
    /////////////////////////////////////////////////////////////
    /// This is the slot that gets called from main to start everything
    /// but, everthing is set up in the Constructor
    /////////////////////////////////////////////////////////////
    void run();
    void greetings();
    void onMessageChanged(const QString message);
    void displayProgress(const QString &path, quint32 percent);

    /////////////////////////////////////////////////////////////
    /// slot that get signal when that application is about to quit
    /////////////////////////////////////////////////////////////
    void aboutToQuitApp();
};
