#pragma once

#include <QObject>
#include <QVersionNumber>

#include "botan_all.h"

class consts : public QObject {
    Q_OBJECT
  public:
    explicit consts(QObject *parent = nullptr);

    static inline int const EXIT_CODE_REBOOT = -123456789;
    static inline QVersionNumber const APP_VERSION{6, 0, 0};
    static inline QString const APP_SHORT_NAME  = "Arsenic";
    static inline QString const APP_LONG_NAME   = "Arsenic " + APP_VERSION.toString();
    static inline QString const APP_DESCRIPTION = "Arsenic - Strong encryption";
    static inline QString const APP_URL         = "https://github.com/Antidote1911";

#ifdef ARSENIC_BOTAN_IS_SYSTEM
    static inline QString const BOTAN_VERSION =
        QString::fromStdString(Botan::version_string()) + QStringLiteral(" (system)");
#else
    static inline QString const BOTAN_VERSION =
        QString::fromStdString(Botan::version_string()) + QStringLiteral(" (embedded)");
#endif

  signals:
};
