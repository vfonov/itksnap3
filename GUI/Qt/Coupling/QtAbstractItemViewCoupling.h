#ifndef QTABSTRACTITEMVIEWCOUPLING_H
#define QTABSTRACTITEMVIEWCOUPLING_H

#include <QtWidgetCoupling.h>
#include "SNAPQtCommon.h"
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QModelIndex>
#include <QStandardItemModel>
#include <QAbstractProxyModel>

/**
 * Some Ideas:
 *
 * 1. Coupling between a selection model and an integer value. Treat the
 *    domain as trivial.
 *
 * 2. Coupling between a model that represents a list of items with descriptors
 *    and a QStandardItemModel. Mapping is through
 */

template <class TAtomic>
class DefaultWidgetValueTraits<TAtomic, QAbstractItemView>
    : public WidgetValueTraitsBase<TAtomic, QAbstractItemView *>
{
public:
  const char *GetSignal()
  {
    return SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &));
  }

  virtual QObject *GetSignalEmitter(QObject *w)
  {
    QAbstractItemView *view = dynamic_cast<QAbstractItemView *>(w);
    return view ? view->selectionModel() : NULL;
  }

  TAtomic GetValue(QAbstractItemView *w)
  {
    // Get the UserData associated with the current item
    return qVariantValue<TAtomic>(w->currentIndex().data(Qt::UserRole));
  }

  void SetValue(QAbstractItemView *w, const TAtomic &value)
  {
    // Unset the current index
   int row = -1;

    // We have to actually find the item
    for(int i = 0; i < w->model()->rowCount(); i++)
      {
      QModelIndex idx = w->model()->index(i, 0);
      if(qVariantValue<TAtomic>(w->model()->data(idx, Qt::UserRole)) == value)
        {
        row = i;
        break;
        }
      }

    // Have we found it?
    QModelIndex index = w->model()->index(row, 0);
    w->setCurrentIndex(index);
  }

  void SetValueToNull(QAbstractItemView *w)
  {
    QModelIndex index = w->model()->index(-1, 0);
    w->setCurrentIndex(index);
  }
};


template <class TItemDomain, class TRowTraits>
class QStandardItemModelWidgetDomainTraits :
    public WidgetDomainTraitsBase<TItemDomain, QAbstractItemView *>
{
public:
  // The information about the item type are taken from the domain
  typedef typename TItemDomain::ValueType AtomicType;
  typedef typename TItemDomain::DescriptorType DescriptorType;
  typedef TItemDomain DomainType;

  // Navigate through proxy models until we find a standard item model in the
  // view. Allows couplings to be installed on views with proxies
  QStandardItemModel *GetTopLevelModel(QAbstractItemView *w)
  {
    QAbstractItemModel *model = w->model();
    while(model)
      {
      QStandardItemModel *msi = dynamic_cast<QStandardItemModel *>(model);
      if(msi)
        return msi;

      QAbstractProxyModel *mpx = dynamic_cast<QAbstractProxyModel *>(model);
      if(mpx)
        model = mpx->sourceModel();
      }
    return NULL;
  }

  void SetDomain(QAbstractItemView *w, const DomainType &domain)
  {
    // Remove everything from the model
    QStandardItemModel *model = GetTopLevelModel(w);
    if(!model)
      return;

    model->clear();
    model->setColumnCount(TRowTraits::columnCount());

    // Populate
    for(typename DomainType::const_iterator it = domain.begin();
        it != domain.end(); ++it)
      {
      // Get the key/value pair
      AtomicType value = domain.GetValue(it);
      const DescriptorType &row = domain.GetDescription(it);

      // Use the row traits to map information to the widget
      QList<QStandardItem *> rlist;
      for(int j = 0; j < model->columnCount(); j++)
        {
        QStandardItem *item = new QStandardItem();
        TRowTraits::createItem(item, j, value, row);
        rlist.append(item);
        model->appendRow(rlist);
        }
      }
  }

  void UpdateDomainDescription(QAbstractItemView *w, const DomainType &domain)
  {
    // Remove everything from the model
    QStandardItemModel *model = GetTopLevelModel(w);
    if(!model)
      return;

    // This is not the most efficient way of doing things, because we
    // are still linearly parsing through the widget and updating rows.
    // But at least the actual modifications to the widget are limited
    // to the rows that have been modified.
    //
    // What would be more efficient is to have a list of ids which have
    // been modified and update only those. Or even better, implement all
    // of this using an AbstractItemModel
    int nrows = model->rowCount();
    for(int i = 0; i < nrows; i++)
      {
      QStandardItem *item = model->item(i);
      AtomicType id = TRowTraits::getItemValue(item);
      typename DomainType::const_iterator it = domain.find(id);
      if(it != domain.end())
        {
        const DescriptorType &row = domain.GetDescription(it);
        for(int j = 0; j <  model->columnCount(); j++)
          TRowTraits::updateItem(model->item(i,j), j, row);
        }
      }
  }

  TItemDomain GetDomain(QAbstractItemView *w)
  {
    // We don't actually pull the widget because the domain is fully specified
    // by the model.
    return DomainType();
  }
};

class ColorLabelToQSIMCouplingRowTraits
{
public:

  static int columnCount() { return 1; }

  static void createItem(QStandardItem *item, int column, LabelType label, const ColorLabel &cl)
  {
    // The description
    QString text(cl.GetLabel());

    // The color
    QColor fill(cl.GetRGB(0), cl.GetRGB(1), cl.GetRGB(2));

    // Icon based on the color
    QIcon ic = CreateColorBoxIcon(16, 16, fill);

    // Create item and set its properties
    item->setIcon(ic);
    item->setText(text);
    item->setData(label, Qt::UserRole);
    item->setData(fill, Qt::UserRole + 1);
    item->setData(text, Qt::EditRole);
  }

  static void updateItem(QStandardItem *item, int column, const ColorLabel &cl)
  {
    // Get the properies and compare them to the color label
    QColor currentFill = qVariantValue<QColor>(item->data(Qt::UserRole+1));
    QColor newFill(cl.GetRGB(0), cl.GetRGB(1), cl.GetRGB(2));

    if(currentFill != newFill)
      {
      QIcon ic = CreateColorBoxIcon(16, 16, newFill);
      item->setIcon(ic);
      item->setData(newFill, Qt::UserRole + 1);
      }

    QString currentText = item->text();
    QString newText(cl.GetLabel());

    if(currentText != newText)
      {
      item->setText(newText);
      item->setData(newText, Qt::EditRole);
      }
  }

  static LabelType getItemValue(QStandardItem *item)
  {
    return qVariantValue<LabelType>(item->data(Qt::UserRole));
  }
};


template<class TAtomic, class TItemDescriptor>
class DefaultQSIMCouplingRowTraits
{
};

template<>
class DefaultQSIMCouplingRowTraits<LabelType, ColorLabel>
    : public ColorLabelToQSIMCouplingRowTraits
{
};

template<class TDomain>
class DefaultWidgetDomainTraits<TDomain, QAbstractItemView>
    : public QStandardItemModelWidgetDomainTraits<
        TDomain, DefaultQSIMCouplingRowTraits<typename TDomain::ValueType,
                                              typename TDomain::DescriptorType> >
{
};

#endif // QTABSTRACTITEMVIEWCOUPLING_H