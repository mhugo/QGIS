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

void QgsLabelLayerCacheTest::invalidate()
{
  mInvalidated = true;
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
  : QgsMapLayer( LabelLayer, layerName ),
    mCacheEnabled( false )
{
  setLegend( new QgsLabelLayerLegend() );
  mValid = true;

  // invalidate cache when triggerRepaint is called, to be consistent with QgsMapLayer's behaviour
  connect( this, SIGNAL( repaintRequested() ), this, SIGNAL( invalidateCache() ) );
}

void QgsLabelLayer::invalidateCache()
{
  std::cout << "invalidate cache" << std::endl;
  mCacheTest.invalidate();
}

bool QgsLabelLayer::cacheEnabled() const
{
  return mCacheEnabled;
}

void QgsLabelLayer::setCacheEnabled( bool enable )
{
  mCacheEnabled = enable;
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

// local exception class used by vectorLayer()
struct CancelRendering {};

// local function that will throw when the given layer does not exist anymore
QgsVectorLayer* vectorLayer( const QString& lid )
{
  QgsMapLayer* ml = QgsMapLayerRegistry::instance()->mapLayer( lid );
  if ( !ml )
  {
    throw CancelRendering();
  }
  return static_cast<QgsVectorLayer*>(ml);
}

bool QgsLabelLayer::draw( QgsRenderContext& context )
{
  bool cancelled = false;

  if ( !context.labelingEngine() ) {
    return false;
  }
  // QgsPalLabeling does not seem to be designed to be reused, so use a local copy
  QScopedPointer<QgsLabelingEngineInterface> pal( context.labelingEngine()->clone() );

  bool nothingToLabel = true;
  QSet<QgsVectorLayer*> layersToTest;
  QSet<QString> layersIdToTest;
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
      layersIdToTest << vl->id();
    }

    if ( context.renderingStopped() )
    {
      return false;
    }
  }

  QImage* img = dynamic_cast<QImage*>( context.painter()->device() );
  QPainter* oldPainter = context.painter();
  QPainter* imgPainter = 0;
  if ( mCacheEnabled && img ) {
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
  try
  {
    foreach( const QString& vlid, layersIdToTest )
    {
      // we call vectorLayer(vlid) each time the vector layer is needed
      // it will then throw an exception if the layer has been deleted by another thread
      // and allows to cancel the rendering
      QStringList attrNames;
      pal->prepareLayer( vectorLayer(vlid), attrNames, context );

      QgsFeature fet;
      // a label layer has no CRS per se (it refers multiple layers), so we need to access labeling settings
      QgsPalLayerSettings& plyr = pal->layer( vlid );
      QgsRectangle dataExtent;
      if ( plyr.ct )
      {
        dataExtent = plyr.ct->transformBoundingBox( context.extent(), QgsCoordinateTransform::ReverseTransform );
      }
      else
      {
        dataExtent = context.extent();
      }
      QgsFeatureRequest req = QgsFeatureRequest().setFilterRect( dataExtent ).setSubsetOfAttributes( attrNames, vectorLayer(vlid)->dataProvider()->fields() );

      QgsFeatureIterator fit = vectorLayer(vlid)->getFeatures( req );
      while ( fit.nextFeature( fet ) ) {
        if ( context.renderingStopped() )
        {
          throw CancelRendering();
        }

        nothingToLabel = false;
        pal->registerFeature( vectorLayer(vlid)->id(), // makes sure the layer still exists
                              fet,
                              context );
      }
    }
  }
  catch ( CancelRendering& )
  {
    cancelled = true;
  }

  if ( !cancelled && !nothingToLabel ) {
    pal->drawLabeling( context );
    pal->exit();
  }

  if ( mCacheEnabled && img) {
    // restore painter
    imgPainter->end();
    context.setPainter( oldPainter );
    oldPainter->drawImage( QPoint(0,0), *mCacheImage );
  }

  return !cancelled;
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
