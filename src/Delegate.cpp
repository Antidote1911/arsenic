/**
 * Kryvos File Encryptor - Encrypts and decrypts files.
 * Copyright (C) 2014 Andrew Dolby
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact : andrewdolby@gmail.com
 */

#include "Delegate.h"
#include <QtCore/QEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QFontMetricsF>

Delegate::Delegate(QObject *parent)
    : QStyledItemDelegate{parent}
    , focusBorderEnabled{false}
{
}

void Delegate::setFocusBorderEnabled(bool enabled)
{
    focusBorderEnabled = enabled;
}

void Delegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    if (!focusBorderEnabled && option->state & QStyle::State_HasFocus) option->state = option->state & ~QStyle::State_HasFocus;
}

void Delegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const int column = index.column();
    switch (column) {
    case 0: {
        QStyleOptionButton buttonOption;
        auto font = QApplication::font();
        buttonOption.state = QStyle::State_Enabled;
        buttonOption.rect = option.rect;
        buttonOption.features = QStyleOptionButton::Flat;
        const QIcon closeIcon("://pixmaps/closeFileIcon.svg");
        buttonOption.icon = closeIcon;
        buttonOption.iconSize = QSize((int)option.rect.width() * 0.4, (int)option.rect.height() * 0.4);

        QApplication::style()->drawControl(QStyle::CE_PushButton, &buttonOption, painter);
        break;
    }

    case 1:
        QStyledItemDelegate::paint(painter, option, index);
        break;

    case 2:
        QStyledItemDelegate::paint(painter, option, index);
        break;

    case 3:
        QStyledItemDelegate::paint(painter, option, index);
        break;

    case 4: {
        // Set up a QStyleOptionProgressBar to mimic the environment of a progress
        // bar.
        QStyleOptionProgressBar bar ;

                bar.rect = option.rect;

                bar.progress = index.data().toInt();
                bar.maximum = 100;
                bar.minimum = 0;
                bar.text = QString::number(bar.progress)+"%";
                bar.textVisible = true;
                QApplication::style()->drawControl(QStyle::CE_ProgressBar,&bar,painter);

        break;
    }
    }
}

bool Delegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (index.column() == 0)
        if ((event->type() == QEvent::MouseButtonRelease) || (event->type() == QEvent::MouseButtonDblClick)) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->button() == Qt::LeftButton) && option.rect.contains(mouseEvent->pos())) emit removeRow(index);
        }
    return (QStyledItemDelegate::editorEvent(event, model, option, index));
}

QSize Delegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(0);
    return (s);
}
