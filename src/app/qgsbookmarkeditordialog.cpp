/***************************************************************************
                         qgsbookmarkeditordialog.cpp
                         -------------------------------------
    begin                : September 2019
    copyright            : (C) 2019 by Mathieu Pellerin
    email                : nirvn dot asia at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsbookmarkeditordialog.h"

#include "qgis.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsextentgroupbox.h"
#include "qgsgui.h"
#include "qgsguiutils.h"
#include "qgsprojectionselectiondialog.h"
#include "qgsproject.h"
#include "qgsmapcanvas.h"

QgsBookmarkEditorDialog::QgsBookmarkEditorDialog( QgsBookmark bookmark, bool inProject, QWidget *parent, QgsMapCanvas *mapCanvas )
  : QDialog( parent )
  , mBookmark( bookmark )
  , mInProject( inProject )
  , mMapCanvas( mapCanvas )
{
  setupUi( this );
  QgsGui::instance()->enableAutoGeometryRestore( this );

  mName->setText( mBookmark.name() );

  QSet<QString> groups = QSet<QString>::fromList( QgsProject::instance()->bookmarkManager()->groups() << QgsApplication::instance()->bookmarkManager()->groups() );
  QStringList groupsList = groups.toList();
  groupsList.removeOne( QString() );
  groupsList.sort();
  mGroup->addItems( groupsList );
  mGroup->setEditText( mBookmark.group() );

  mExtentGroupBox->setOutputCrs( mBookmark.extent().crs() );
  mExtentGroupBox->setCurrentExtent( mBookmark.extent(), mBookmark.extent().crs() );
  mExtentGroupBox->setOutputExtentFromCurrent();
  mExtentGroupBox->setMapCanvas( mMapCanvas );
  mCrsSelector->setCrs( mBookmark.extent().crs() );

  mSaveLocation->addItem( tr( "User Bookmarks" ), ApplicationManager );
  mSaveLocation->addItem( tr( "Project Bookmarks" ), ProjectManager );
  mSaveLocation->setCurrentIndex( mSaveLocation->findData( mInProject ? ProjectManager : ApplicationManager ) );

  connect( mCrsSelector, &QgsProjectionSelectionWidget::crsChanged, this, &QgsBookmarkEditorDialog::crsChanged );
  connect( buttonBox, &QDialogButtonBox::accepted, this, &QgsBookmarkEditorDialog::onAccepted );

  mName->setFocus();
}

void QgsBookmarkEditorDialog::crsChanged( const QgsCoordinateReferenceSystem &crs )
{
  mExtentGroupBox->setOutputCrs( crs );
}

void QgsBookmarkEditorDialog::onAccepted()
{
  QgsBookmark bookmark;
  bookmark.setId( mBookmark.id() );
  bookmark.setName( mName->text() );
  bookmark.setGroup( mGroup->currentText() );
  bookmark.setExtent( QgsReferencedRectangle( mExtentGroupBox->outputExtent(), mExtentGroupBox->outputCrs() ) );

  if ( bookmark.id().isEmpty() )
  {
    // Creating a new bookmark
    if ( mSaveLocation->currentData() == ProjectManager )
      QgsProject::instance()->bookmarkManager()->addBookmark( bookmark );
    else
      QgsApplication::instance()->bookmarkManager()->addBookmark( bookmark );
  }
  else
  {
    // Editing a pre-existing bookmark
    if ( mInProject )
      QgsProject::instance()->bookmarkManager()->updateBookmark( bookmark );
    else
      QgsApplication::instance()->bookmarkManager()->updateBookmark( bookmark );

    if ( !mInProject && mSaveLocation->currentData() == ProjectManager )
      QgsApplication::instance()->bookmarkManager()->moveBookmark( bookmark.id(), QgsProject::instance()->bookmarkManager() );
    else if ( mInProject && mSaveLocation->currentData() == ApplicationManager )
      QgsProject::instance()->bookmarkManager()->moveBookmark( bookmark.id(), QgsApplication::instance()->bookmarkManager() );
  }
}

