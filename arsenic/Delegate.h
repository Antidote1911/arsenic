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

#ifndef KRYVOS_GUI_DELEGATE_HPP_
#define KRYVOS_GUI_DELEGATE_HPP_

#include <QtWidgets/QStyledItemDelegate>
#include <QtCore/QSize>

/*!
 * \brief The Delegate class
 */
class Delegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /*!
     * \brief Delegate The ProgressDelegate displays a progress bar in a
     * model view.
     * \param parent
     */
    explicit Delegate(QObject* parent = nullptr);

signals:
    void removeRow(const QModelIndex& index);

public:
    /*!
     * \brief setFocusBorderEnabled Enables/disables the focus dotted line border
     * that appears on click.
     * \param enabled The boolean representing whether to enable or disable the
     * focus border.
     */
    void setFocusBorderEnabled(bool enabled);

protected:
    /*!
     * \brief initStyleOption Initializes the style option for this delegate. Used
     * to remove the focus
     * \param option The style view options object.
     * \param index The index of the current cell in the model.
     */
    virtual void initStyleOption(QStyleOptionViewItem* option,
                                 const QModelIndex& index) const;

    /*!
     * \brief paint Paints the progress bar and the close button.
     * \param painter The painter object.
     * \param option The style view options object. Passed on to parent's paint.
     * \param index The index of the current cell in the model to paint.
     */
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;

    /*!
     * \brief editorEvent Event to handle mouse clicks on the remove button.
     * \param event The editing event.
     * \param model The model.
     * \param option The style option view item.
     * \param index The index in the model.
     * \return
     */
    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index);

    /*!
     * \brief sizeHint Returns the size of this delegate.
     * \param option The style view options object.
     * \param index
     * \return
     */
    virtual QSize sizeHint(const QStyleOptionViewItem& option,
                           const QModelIndex& index) const;

protected:
    bool focusBorderEnabled;
};

#endif // KRYVOS_GUI_DELEGATE_HPP_
