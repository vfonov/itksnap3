/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: Filename.cxx,v $
  Language:  C++
  Date:      $Date: 2010/10/18 11:25:44 $
  Version:   $Revision: 1.12 $
  Copyright (c) 2011 Paul A. Yushkevich

  This file is part of ITK-SNAP

  ITK-SNAP is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

=========================================================================*/

#include "QtWidgetActivator.h"
#include "LatentITKEventNotifier.h"
#include <QWidget>
#include <QAction>
#include <StateManagement.h>
#include "GlobalUIModel.h"
#include "SNAPUIFlag.h"

QtWidgetActivator
::QtWidgetActivator(QObject *parent, QList<QObject *> widgets, BooleanCondition *cond, Options options)
  : QObject(parent)
{
  // Map the inputs to widgets and actions
  for(auto *obj : widgets)
    {
    if(dynamic_cast<QWidget *>(obj))
      m_TargetWidgets.push_back(dynamic_cast<QWidget *>(obj));
    else if(dynamic_cast<QAction *>(obj))
      m_TargetActions.push_back(dynamic_cast<QAction *>(obj));
    }

  // Store condition and options
  m_Condition = cond;
  m_Options = options;

  // Give it a name
  setObjectName(QString("Activator:%1").arg(widgets[0]->objectName()));

  // React to events after control returns to the main UI loop
  LatentITKEventNotifier::connect(
        cond, StateMachineChangeEvent(), this, SLOT(OnStateChange(const EventBucket &)));

  // Update the state of the widget
  EventBucket dummy;
  this->OnStateChange(dummy);
}


QtWidgetActivator::~QtWidgetActivator()
{
}

void QtWidgetActivator::OnStateChange(const EventBucket &)
{
  // Update the state of the widget based on the condition
  bool active = (*m_Condition)();
  for(auto *widget : m_TargetWidgets)
    {
    bool status = widget->isEnabledTo(widget->parentWidget());
    if(status != active)
      {
      widget->setEnabled(active);
      if(m_Options & HideInactive)
        widget->setVisible(active);
      }
    }
  for(auto *action : m_TargetActions)
    {
    bool status = action->isEnabled();
    if(status != active)
      {
      action->setEnabled(active);
      if(m_Options & HideInactive)
        action->setVisible(active);
      }
    }
}
