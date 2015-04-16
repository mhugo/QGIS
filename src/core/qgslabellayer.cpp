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

bool QgsLabelLayerCacheTest::test( QgsRectangle extent, double scale, const QSet<QgsVectorLayer*>& layers )
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
  foreach( QgsVectorLayer* vl, layers ) {
    layerIds << vl->id();
  }
  if ( hit ) {
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
  mInvalidated = false;

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

QgsLabelLayer* QgsLabelLayer::mainLabelLayer()
{
  static QgsLabelLayer mainLayer;
  static bool init = false;
  if (!init) {
    // this is a special layer, with a special id
    mainLayer.mID = "_mainlabels_";
    init = true;
  }
  return &mainLayer;
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

    if ( ((vl->labelLayer().isEmpty() && id() == "_mainlabels_") ||
          (!vl->labelLayer().isEmpty() && id() == vl->labelLayer() )) &&
         pal->willUseLayer( vl ) )
    {
      layersToTest << vl;
    }
  }

  // context.extent only gives the requested extent for redraw
  // we need the visible extent

  // FIXME toMapCoordinates works when rotation are used
  // but might be a bit slow (matrix inversion inside)
  // consider using a simpler computation when non rotation ?
  QgsMapToPixel p = context.mapToPixel();
  QgsPoint p1 = p.toMapCoordinates( QPoint( 0, 0 ) );
  QgsPoint p2 = p.toMapCoordinates( QPoint( 0, p.mapHeight() ) );
  QgsPoint p3 = p.toMapCoordinates( QPoint( p.mapWidth(), 0 ) );
  QgsPoint p4 = p.toMapCoordinates( QPoint( p.mapWidth(), p.mapHeight() ) );

  double x0 = std::min( p1.x(), std::min( p2.x(), std::min( p3.x(), p4.x() ) ) );
  double y0 = std::min( p1.y(), std::min( p2.y(), std::min( p3.y(), p4.y() ) ) );
  double x1 = std::max( p1.x(), std::max( p2.x(), std::max( p3.x(), p4.x() ) ) );
  double y1 = std::max( p1.y(), std::max( p2.y(), std::max( p3.y(), p4.y() ) ) );
  QgsRectangle visibleExtent( x0, y0, x1, y1 );

  // copy the context to a local context
  // and fix the extent to the visible extent
  QgsRenderContext localContext( context );
  localContext.setExtent( visibleExtent );

  QImage* img = dynamic_cast<QImage*>( localContext.painter()->device() );
  QPainter* oldPainter = localContext.painter();
  QPainter* imgPainter = 0;
  if (img) {
    std::cout << "device is an image" << std::endl;
    if ( mCacheTest.test( visibleExtent, localContext.scaleFactor(), layersToTest ) ) {
      // we have a cache image, use it
      std::cout << "use cache image" << std::endl;
      localContext.painter()->drawImage( QPoint(0,0), *mCacheImage );
      return true;
    }
    mCacheImage.reset( new QImage( img->width(), img->height(), img->format() ) );
    mCacheImage->fill( QColor(0,0,0,0) );
    imgPainter = new QPainter( mCacheImage.data() );
    localContext.setPainter( imgPainter );
  }

  // draw labels
  foreach( QgsVectorLayer* vl, layersToTest ) {
    QStringList attrNames;
    pal->prepareLayer( vl, attrNames, localContext );

    QgsFeature fet;
    // a label layer has no CRS per se (it refers multiple layers), so we need to access labeling settings
    QgsPalLayerSettings& plyr = pal->layer( vl->id() );
    QgsRectangle dataExtent;
    if ( plyr.ct )
    {
      dataExtent = plyr.ct->transformBoundingBox( visibleExtent, QgsCoordinateTransform::ReverseTransform );
    }
    else
    {
      dataExtent = visibleExtent;
    }
    QgsFeatureRequest req = QgsFeatureRequest().setFilterRect( dataExtent ).setSubsetOfAttributes( attrNames, vl->dataProvider()->fields() );

    QgsFeatureIterator fit = vl->getFeatures( req );
    while ( fit.nextFeature( fet ) ) {
      nothingToLabel = false;
      pal->registerFeature( vl->id(), fet, localContext );
    }
  }

  if ( !nothingToLabel ) {
    pal->drawLabeling( localContext );
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
