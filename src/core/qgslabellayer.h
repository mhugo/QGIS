/***************************************************************************
    qgslabellayer.h
    ---------------------
    begin                : April 2015
    copyright            : (C) 2015 by Hugo Mercier / Oslandia
    email                : hugo dot mercier at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSLABELLAYER_H
#define QGSLABELLAYER_H

#include "qgsmaplayer.h"
#include "qgsmaplayerlegend.h"

class QgsLabelLayerLegend;
/** \ingroup core
    Label layer class
 */
class CORE_EXPORT QgsLabelLayer : public QgsMapLayer
{
    Q_OBJECT

  public:
    static const QString MainLabelId;

    QgsLabelLayer( QString layerName = "" );
    ~QgsLabelLayer();

    /**
     * Rendering part
     */
    virtual bool draw( QgsRenderContext& rendererContext ) override;

    virtual QgsMapLayerRenderer* createMapRenderer( QgsRenderContext& rendererContext ) override;

    virtual bool readSymbology( const QDomNode& /*node*/, QString& /*errorMessage*/) override
    {
      return true;
    }

    virtual bool writeSymbology( QDomNode& /*node*/, QDomDocument& /*doc*/, QString& /*errorMessage*/) const override
    {
      return true;
    }

    bool readXml( const QDomNode & layer_node ) override;
    bool writeXml( QDomNode & layer_node, QDomDocument & document ) override;

    QSet<QgsVectorLayer*> vectorLayers() const;

    static QgsLabelLayer* mainLabelLayer();

 private slots:
    void onLayersAdded( QList<QgsMapLayer*> );
    void onLayerRemoved( QString layerid );
    void onLabelLayerChanged( const QString& oldLabelLayer );

 private:
    bool mInit;

    void init();

    void prepareDiagrams( QgsVectorLayer* layer, QStringList& attributeNames, QgsLabelingEngineInterface* labelingEngine );

    void updateLegend();

    // list of vector layers in this label layer
    QSet<QgsVectorLayer*> mLayers;

    // add a layer to the list of layers, if possible
    // returns true if a layer has been added
    bool addLayer( QgsVectorLayer* );

    QgsLabelLayerLegend* mLegend;
};

namespace QgsLabelLayerUtils
{

bool hasBlendModes( const QgsLabelLayer* layer );

}

/**
 * Private class for label layer legend
 */
class QgsLabelLayerLegend : public QgsMapLayerLegend
{
  Q_OBJECT

public:
  QgsLabelLayerLegend( QgsLabelLayer* layer );

  virtual QList<QgsLayerTreeModelLegendNode*> createLayerTreeModelLegendNodes( QgsLayerTreeLayer* nodeLayer ) override;

private:
  void emitItemsChanged();

  QgsLabelLayer* mLayer;

  // allow it to call emitItemsChanged()
  friend class QgsLabelLayer;
};

#endif // QGSLABELLAYER_H
