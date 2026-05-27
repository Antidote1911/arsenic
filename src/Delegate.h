#pragma once

#include <QtCore/QSize>
#include <QtWidgets/QStyledItemDelegate>

/*!
 * \brief The Delegate class
 */
class Delegate : public QStyledItemDelegate {
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
