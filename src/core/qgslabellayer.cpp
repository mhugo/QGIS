/***************************************************************************
    qgslabellayer.cpp
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
#include "qgslabellayer.h"

#include "qgsmaplayerlegend.h"
#include "qgsmaplayerrenderer.h"
#include "qgslayertreemodellegendnode.h"
#include "qgspallabeling.h"
#include "qgsvectordataprovider.h"
#include "qgsmaplayerregistry.h"
#include "qgsgeometry.h"

bool QgsLabelLayerCacheTest::test( QgsRectangle extent, double scale, QSet<QgsVectorLayer*> layers )
{
  bool hit = true;
  if ( scale != mScale ) {
    hit = false;
  }
  if ( hit && extent != mExtent ) {
    hit = false;
  }
  // now compare layers
  QSet<QString> layerIds;
  if ( hit ) {
    foreach( QgsVectorLayer* vl, layers ) {
      layerIds << vl->id();
    }
    if ( layerIds != mLayers ) {
      hit = false;
    }
    else {
      hit = !mInvalidated;
    }
  }

  // save parameters for the next call
  mScale = scale;
  mExtent = extent;

  disconnectLayers();
  mLayers = layerIds;
  // connect new layers
  foreach( QgsVectorLayer* vl, layers ) {
    connect( vl, SIGNAL( repaintRequested() ), this, SLOT( onRepaintLayer() ) );
  }

  return hit;
}

void QgsLabelLayerCacheTest::disconnectLayers()
{
  foreach( const QString& lid, mLayers ) {
    QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(QgsMapLayerRegistry::instance()->mapLayer( lid ));
    disconnect( vl, SIGNAL( repaintRequested() ), this, SLOT( onRepaintLayer() ) );
  }
}

QgsLabelLayerCacheTest::~QgsLabelLayerCacheTest()
{
  disconnectLayers();
}

void QgsLabelLayerCacheTest::onRepaintLayer()
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer*>(sender());
  if ( mLayers.contains(vl->id()) ) {
    mInvalidated = true;
  }
}


class QgsLabelLayerLegend : public QgsMapLayerLegend
{
public:
  virtual QList<QgsLayerTreeModelLegendNode*> createLayerTreeModelLegendNodes( QgsLayerTreeLayer* nodeLayer ) override
  {
    QList<QgsLayerTreeModelLegendNode*> nodes;

    nodes << new QgsSimpleLegendNode( nodeLayer, "Labels" );
    return nodes;
  }
};

QgsLabelLayer::QgsLabelLayer( QString layerName )
    : QgsMapLayer( LabelLayer, layerName )
{
  setLegend( new QgsLabelLayerLegend() );
  mValid = true;
}

bool QgsLabelLayer::draw( QgsRenderContext& context )
{
  if ( !context.labelingEngine() ) {
    return false;
  }
  // QgsPalLabeling does not seem to be designed to be reused, so use a local copy
  QScopedPointer<QgsLabelingEngineInterface> pal( context.labelingEngine()->clone() );

  bool nothingToLabel = true;
  QSet<QgsVectorLayer*> layersToTest;
  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() ) {
    if ( ml->type() != QgsMapLayer::VectorLayer ) {
      continue;
    }
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);
    if ( vl->labelLayer() != name() ) {
        continue;
    }
    if ( !pal->willUseLayer( vl ) ) {
      continue;
    }

    layersToTest << vl;
  }

  QImage* img = dynamic_cast<QImage*>( context.painter()->device() );
  QPainter* oldPainter = context.painter();
  QPainter* imgPainter = 0;
  if (img) {
    std::cout << "device is an image" << std::endl;
    if ( mCacheTest.test( context.extent(), context.scaleFactor(), layersToTest ) ) {
      // we have a cache image, use it
      std::cout << "use cache image" << std::endl;
      context.painter()->drawImage( QPoint(0,0), *mCacheImage );
      return true;
    }
    mCacheImage.reset( new QImage( img->width(), img->height(), img->format() ) );
    mCacheImage->fill( QColor(0,0,0,0) );
    imgPainter = new QPainter( mCacheImage.data() );
    context.setPainter( imgPainter );
  }

  // draw labels
  foreach( QgsVectorLayer* vl, layersToTest ) {
    QStringList attrNames;
    pal->prepareLayer( vl, attrNames, context );

    QgsFeature fet;
    QgsRectangle rect = context.extent();

    // a label layer has no CRS per se (it refers multiple layers), so we need to access labeling settings
    QgsPalLayerSettings& plyr = pal->layer( vl->id() );
    if ( plyr.ct ) {
      rect = plyr.ct->transformBoundingBox( context.extent(), QgsCoordinateTransform::ReverseTransform );
    }
    else {
      rect = context.extent();
    }
    QgsFeatureRequest req = QgsFeatureRequest().setFilterRect( rect ).setSubsetOfAttributes( attrNames, vl->dataProvider()->fields() );

    QgsFeatureIterator fit = vl->getFeatures( req );
    while ( fit.nextFeature( fet ) ) {
      nothingToLabel = false;
      pal->registerFeature( vl->id(), fet, context );
    }
  }

  if ( !nothingToLabel ) {
    pal->drawLabeling( context );
    pal->exit();
  }

  if (img) {
    // restore painter
    imgPainter->end();
    context.setPainter( oldPainter );
    oldPainter->drawImage( QPoint(0,0), *mCacheImage );
  }

  return true;
}

class QgsLabelLayerRenderer : public QgsMapLayerRenderer
{
  public:
    QgsLabelLayerRenderer( QgsLabelLayer* layer, QgsRenderContext& rendererContext )
        : QgsMapLayerRenderer( layer->id() )
        , mLayer( layer )
        , mRendererContext( rendererContext )
    {}

    virtual bool render() override
    {
      return mLayer->draw( mRendererContext );
    }

  protected:
    QgsLabelLayer* mLayer;
    QgsRenderContext& mRendererContext;
};

QgsMapLayerRenderer* QgsLabelLayer::createMapRenderer( QgsRenderContext& rendererContext )
{
  return new QgsLabelLayerRenderer( this, rendererContext );
}
