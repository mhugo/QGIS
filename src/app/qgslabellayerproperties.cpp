/***************************************************************************
                          qgslabellayerproperties.h
                   Property dialog for label layers
                             -------------------
    begin                : 2015-04-21
    copyright            : (C) 2015 by Hugo Mercier / Oslandia
    email                : hugo dot mercier at oslandia dot com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QPushButton>

#include "qgslabellayerproperties.h"
#include "qgslabellayer.h"
#include "qgsproject.h"

QgsLabelLayerProperties::QgsLabelLayerProperties( QgsLabelLayer *lyr, QWidget *parent, Qt::WindowFlags fl )
  : QgsOptionsDialogBase( "LabelLayerProperties", parent, fl )
  , mLayer(lyr)
{
  setupUi( this );
  // QgsOptionsDialogBase handles saving/restoring of geometry, splitter and current tab states,
  // switching vertical tabs between icon/text to icon-only modes (splitter collapsed to left),
  // and connecting QDialogButtonBox's accepted/rejected signals to dialog's accept/reject slots
  initOptionsBase( false );

  const QPushButton* applyBtn = buttonBox->button( QDialogButtonBox::Apply );
  connect( applyBtn, SIGNAL( clicked() ), this, SLOT( apply() ) );
  connect( this, SIGNAL( accepted() ), this, SLOT( apply() ) );

  mLayerNameLineEdit->setText( mLayer->originalName() );
  mBlendModeComboBox->setBlendMode( mLayer->blendMode() );

  QSettings settings;
  // if dialog hasn't been opened/closed yet, default to Styles tab, which is used most often
  // this will be read by restoreOptionsBaseUi()
  if ( !settings.contains( QString( "/Windows/LabelLayerProperties/tab" ) ) )
  {
    settings.setValue( QString( "/Windows/LabelLayerProperties/tab" ),
                       mOptStackedWidget->indexOf( mOptsPage_Style ) );
  }

  if ( QgsLabelLayerUtils::hasBlendModes( mLayer ) )
  {
    mBlendModeGroupBox->setEnabled( false );
    mBlendModeGroupBox->setToolTip( tr( "Some labels are defined with blend modes for their text, buffer, shape or shadow. Layer blend modes will be ignored." ) );
  }
  else
  {
    mBlendModeGroupBox->setEnabled( true );
    mBlendModeGroupBox->setToolTip( "" );
  }

  QString title = QString( tr( "Layer Properties - %1" ) ).arg( lyr->name() );
  restoreOptionsBaseUi( title );
}

QgsLabelLayerProperties::~QgsLabelLayerProperties()
{
}

void QgsLabelLayerProperties::apply()
{
  mLayer->setLayerName( mLayerNameLineEdit->text() );
  if ( mBlendModeGroupBox->isEnabled() )
  {
    mLayer->setBlendMode( mBlendModeComboBox->blendMode() );
  }
  else
  {
    mLayer->setBlendMode( QPainter::CompositionMode_SourceOver );
  }

  mLayer->triggerRepaint();
  // notify the project we've made a change
  QgsProject::instance()->dirty( true );
}
