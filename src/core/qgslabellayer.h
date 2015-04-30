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
    /**
     * The main label layer id
     */
    static const QString MainLayerId;

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

    /**
     * Returns the current list of vector layers that refers to this label layer (through QgsVectorLayer::setLabelLayer)
     * The returned list is guaranteed to have a stable order
     */
    QList<QgsVectorLayer*> vectorLayers() const;

    /**
     * Returns (and creates if needed) the "main" label layer that is always
     * added is no label layers are explictly created
     */
    static QgsLabelLayer* mainLabelLayer();

 private slots:
    void onLayersAdded( QList<QgsMapLayer*> );
    void onLayerRemoved( QString layerid );
    void onLabelLayerChanged( const QString& oldLabelLayer );
    void onLayerRenamed();

 signals:
    /** Emitted when the list of vector layers changes */
    void layersChanged();

 private:
    bool mInit;

    void init();

    void prepareDiagrams( QgsVectorLayer* layer, QStringList& attributeNames, QgsLabelingEngineInterface* labelingEngine );

    // list of vector layers in this label layer
    QList<QgsVectorLayer*> mLayers;

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
  QgsLabelLayer* mLayer;
};

#endif // QGSLABELLAYER_H
