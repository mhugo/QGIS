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

class QgsLabelLayerCacheTest : public QObject
{
  Q_OBJECT

public:

  QgsLabelLayerCacheTest() : mInvalidated(false) {}
  ~QgsLabelLayerCacheTest();

  //
  // Cache test. Returns true if the last call was with the same parameters
  // and with vector layers not invalidated
  bool test( QgsRectangle extent, double scale, const QSet<QgsVectorLayer*>& layers );

  // force invalidation
  void invalidate();

private slots:
  void onRepaintLayer();

private:
  // list of layer' id
  QSet<QString> mLayers;
  // extent
  QgsRectangle mExtent;
  // scale
  double mScale;

  void disconnectLayers();

  bool mInvalidated;
};

/** \ingroup core
    Label layer class
 */
class CORE_EXPORT QgsLabelLayer : public QgsMapLayer
{
    Q_OBJECT

  public:
    QgsLabelLayer( QString layerName = "" );

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

    bool cacheEnabled() const;

    void setCacheEnabled( bool e );

    static QgsLabelLayer* mainLabelLayer();

 private slots:
    void invalidateCache();

 private:
    void prepareDiagrams( QgsVectorLayer* layer, QStringList& attributeNames, QgsLabelingEngineInterface* labelingEngine );

    QVector<QgsVectorLayer*> mLayers;

    QgsLabelLayerCacheTest mCacheTest;

    QScopedPointer<QImage> mCacheImage;

    bool mCacheEnabled;
};


/**
 * Private class for label layer legend
 */
class QgsLabelLayerLegend : public QgsMapLayerLegend
{
  Q_OBJECT

public:
  QgsLabelLayerLegend( QgsLabelLayer* layer );

  virtual QList<QgsLayerTreeModelLegendNode*> createLayerTreeModelLegendNodes( QgsLayerTreeLayer* nodeLayer ) override;

private slots:
  void onLayersAdded( QList<QgsMapLayer*> layers );
  void onLayerRemoved( QString );
  void onLabelLayerChanged( const QString& oldLabel );

private:
  QgsLabelLayer* mLayer;
};

#endif // QGSLABELLAYER_H
