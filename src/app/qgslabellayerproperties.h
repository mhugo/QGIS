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

#ifndef QGSLABELLAYERPROPERTIES
#define QGSLABELLAYERPROPERTIES

#include "ui_qgslabellayerpropertiesbase.h"
#include "qgsoptionsdialogbase.h"

class QgsLabelLayer;

class APP_EXPORT QgsLabelLayerProperties : public QgsOptionsDialogBase, private Ui::QgsLabelLayerPropertiesBase
{
    Q_OBJECT

  public:
    QgsLabelLayerProperties( QgsLabelLayer *lyr = 0, QWidget *parent = 0, Qt::WindowFlags fl = QgisGui::ModalDialogFlags );

    ~QgsLabelLayerProperties();

  public slots:

    /** Called when apply button is pressed or dialog is accepted */
    void apply();

  private:
    QgsLabelLayer* mLayer;
};

#endif
