#include "skin.h"
#include <QStyle>
#include <QApplication>
#include <QStyleFactory>
#include <QPointer>

/*******************************************************************************

*******************************************************************************/

Skin::Skin(QObject *parent)
    : QObject(parent)
{
}

Skin::~Skin()
{
}

QPointer<Skin> Skin::m_instance(nullptr);


void Skin::setSkin(const QString &key)
{

    if (key == "dark") {

        // modify palette to dark
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);

        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, Qt::gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Base, QColor(15, 15, 15));
        darkPalette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(33, 33, 33));
        darkPalette.setColor(QPalette::Disabled, QPalette::ToolTipBase, Qt::gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::ToolTipText, Qt::gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, Qt::gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Button, QColor(33, 33, 33));
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Link, QColor(22, 110, 198));
        darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(22, 110, 198));

        qApp->setPalette(darkPalette);
        qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
    }
    else if (key == "light") {
        QPalette lightPalette;
        lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::black);
        lightPalette.setColor(QPalette::ToolTipText, Qt::white);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);


        qApp->setPalette(lightPalette);
    }
    else if (key == "classic") {
        qApp->setPalette(qApp->style()->standardPalette());
    }
}

Skin *Skin::instance()
{
    if (!m_instance) {
        m_instance = new Skin(qApp);
    }

    return m_instance;
}
