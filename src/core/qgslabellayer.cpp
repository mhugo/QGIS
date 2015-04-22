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

#include "qgsmaplayerrenderer.h"
#include "qgslayertreemodellegendnode.h"
#include "qgspallabeling.h"
#include "qgsvectordataprovider.h"
#include "qgsmaplayerregistry.h"
#include "qgsgeometry.h"
#include "qgsdataitem.h"
#include "qgsmessagelog.h"
#include "diagram/qgsdiagram.h"
#include "symbology-ng/qgsrendererv2.h"

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


QgsLabelLayerLegend::QgsLabelLayerLegend( QgsLabelLayer* layer ) : QgsMapLayerLegend(), mLayer(layer)
{
  // register new layer creatio / deletionn, to update the legend
  connect( QgsMapLayerRegistry::instance(), SIGNAL( legendLayersAdded(QList<QgsMapLayer*>) ), this, SLOT( onLayersAdded(QList<QgsMapLayer*>) ) );

  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() )
  {
    if ( ml->type() != QgsMapLayer::VectorLayer )
    {
      continue;
    }

    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);
    // register to label layer change
    connect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
    // register to the deletion
    connect( QgsMapLayerRegistry::instance(), SIGNAL( layerRemoved(QString) ), this, SLOT( onLayerRemoved(QString) ) );
  }
}

QList<QgsLayerTreeModelLegendNode*> QgsLabelLayerLegend::createLayerTreeModelLegendNodes( QgsLayerTreeLayer* nodeLayer )
{
  QList<QgsLayerTreeModelLegendNode*> nodes;

  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() ) {
    if ( ml->type() != QgsMapLayer::VectorLayer ) {
      continue;
    }
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);

    if ( !vl->labelLayer().isEmpty() && mLayer->id() == vl->labelLayer() )
    {
      QIcon icon;
      if ( vl->geometryType() == QGis::Point )
      {
        icon = QgsLayerItem::iconPoint();
      }
      else if ( vl->geometryType() == QGis::Line )
      {
        icon = QgsLayerItem::iconLine();
      }
      else if ( vl->geometryType() == QGis::Polygon )
      {
        icon = QgsLayerItem::iconPolygon();
      }
      else
      {
        icon = QgsLayerItem::iconDefault();
      }

      QgsLayerTreeModelLegendNode* node = new QgsSimpleLegendNode( nodeLayer, vl->name(), icon );
      nodes << node;
    }

  }
  return nodes;
}

void QgsLabelLayerLegend::onLayersAdded( QList<QgsMapLayer*> layers )
{
  bool doEmit = false;
  foreach( QgsMapLayer* ml, layers )
  {
    if ( ml->type() != QgsMapLayer::VectorLayer )
    {
      return;
    }

    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);
    if ( vl->labelLayer() == mLayer->id() )
    {
      doEmit = true;
    }
    // register to label layer change
    connect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
    // register to the deletion
    connect( QgsMapLayerRegistry::instance(), SIGNAL( layerRemoved(QString) ), this, SLOT( onLayerRemoved(QString) ) );
  }
  if (doEmit)
  {
    emit itemsChanged();
  }
}

void QgsLabelLayerLegend::onLabelLayerChanged( const QString& oldLabel )
{
  QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(sender());
  if ( oldLabel == mLayer->id() || vl->labelLayer() == mLayer->id() )
  {
    emit itemsChanged();
  }
}

void QgsLabelLayerLegend::onLayerRemoved( QString layerId )
{
  // disconnect label layer change signal
  QgsVectorLayer *vl = static_cast<QgsVectorLayer*>(QgsMapLayerRegistry::instance()->mapLayer( layerId ));
  disconnect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );

  emit itemsChanged();
}

QgsLabelLayer::QgsLabelLayer( QString layerName )
  : QgsMapLayer( LabelLayer, layerName ),
    mCacheEnabled( false )
{
  setLegend( new QgsLabelLayerLegend(this) );
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

void QgsLabelLayer::prepareDiagrams( QgsVectorLayer* layer, QStringList& attributeNames, QgsLabelingEngineInterface* labelingEngine )
{
  if ( !layer->diagramRenderer() || !layer->diagramLayerSettings() )
    return;

  const QgsDiagramRendererV2* diagRenderer = layer->diagramRenderer();
  const QgsDiagramLayerSettings* diagSettings = layer->diagramLayerSettings();

  QgsFields fields = layer->pendingFields();

  labelingEngine->addDiagramLayer( layer, diagSettings ); // will make internal copy of diagSettings + initialize it

  //add attributes needed by the diagram renderer
  QList<QString> att = diagRenderer->diagramAttributes();
  QList<QString>::const_iterator attIt = att.constBegin();
  for ( ; attIt != att.constEnd(); ++attIt )
  {
    QgsExpression* expression = diagRenderer->diagram()->getExpression( *attIt, &fields );
    QStringList columns = expression->referencedColumns();
    QStringList::const_iterator columnsIterator = columns.constBegin();
    for ( ; columnsIterator != columns.constEnd(); ++columnsIterator )
    {
      if ( !attributeNames.contains( *columnsIterator ) )
        attributeNames << *columnsIterator;
    }
  }

  const QgsLinearlyInterpolatedDiagramRenderer* linearlyInterpolatedDiagramRenderer = dynamic_cast<const QgsLinearlyInterpolatedDiagramRenderer*>( layer->diagramRenderer() );
  if ( linearlyInterpolatedDiagramRenderer != NULL )
  {
    if ( linearlyInterpolatedDiagramRenderer->classificationAttributeIsExpression() )
    {
      QgsExpression* expression = diagRenderer->diagram()->getExpression( linearlyInterpolatedDiagramRenderer->classificationAttributeExpression(), &fields );
      QStringList columns = expression->referencedColumns();
      QStringList::const_iterator columnsIterator = columns.constBegin();
      for ( ; columnsIterator != columns.constEnd(); ++columnsIterator )
      {
        if ( !attributeNames.contains( *columnsIterator ) )
          attributeNames << *columnsIterator;
      }
    }
    else
    {
      QString name = fields.at( linearlyInterpolatedDiagramRenderer->classificationAttribute() ).name();
      if ( !attributeNames.contains( name ) )
        attributeNames << name;
    }
  }

  //and the ones needed for data defined diagram positions
  if ( diagSettings->xPosColumn != -1 )
    attributeNames << fields.at( diagSettings->xPosColumn ).name();
  if ( diagSettings->yPosColumn != -1 )
    attributeNames << fields.at( diagSettings->yPosColumn ).name();
}

bool QgsLabelLayer::draw( QgsRenderContext& context )
{
  bool cancelled = false;

  if ( !context.labelingEngine() )
  {
    return false;
  }
  // QgsPalLabeling does not seem to be designed to be reused, so use a local copy
  QScopedPointer<QgsLabelingEngineInterface> pal( context.labelingEngine()->clone() );

  bool nothingToLabel = true;
  QSet<QgsVectorLayer*> layersToTest;
  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() )
  {
    if ( ml->type() != QgsMapLayer::VectorLayer )
    {
      continue;
    }
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);

    if ( ((vl->labelLayer().isEmpty() && id() == "_mainlabels_") ||
          (!vl->labelLayer().isEmpty() && id() == vl->labelLayer() )) &&
         (pal->willUseLayer( vl ) || (vl->diagramRenderer() && vl->diagramLayerSettings())) )
    {
      layersToTest << vl;
    }

    if ( context.renderingStopped() )
    {
      return false;
    }
  }

  QImage* img = dynamic_cast<QImage*>( context.painter()->device() );
  QPainter* oldPainter = context.painter();
  QScopedPointer<QPainter> imgPainter;
  if ( mCacheEnabled && img )
  {
    std::cout << "device is an image" << std::endl;
    if ( mCacheTest.test( context.extent(), context.scaleFactor(), layersToTest ) )
    {
      // we have a cache image, use it
      std::cout << "use cache image" << std::endl;
      context.painter()->drawImage( QPoint(0,0), *mCacheImage );
      return true;
    }
    mCacheImage.reset( new QImage( img->width(), img->height(), img->format() ) );
    mCacheImage->fill( QColor(0,0,0,0) );
    imgPainter.reset( new QPainter( mCacheImage.data() ) );
    context.setPainter( imgPainter.data() );
  }

  // draw labels
  foreach( QgsVectorLayer* vl, layersToTest )
  {
    bool hasLabels = false;
    bool hasDiagrams = false;
    // we call vectorLayer(vlid) each time the vector layer is needed
    // it will then throw an exception if the layer has been deleted by another thread
    // and allows to cancel the rendering
    QStringList attrNames;
    if ( pal->willUseLayer( vl ) )
    {
      hasLabels = true;
      pal->prepareLayer( vl, attrNames, context );
    }

    if ( vl->diagramRenderer() && vl->diagramLayerSettings() )
    {
      hasDiagrams = true;
      prepareDiagrams( vl, attrNames, pal.data() );
    }

    QgsFeatureRendererV2* renderer = vl->rendererV2();
    bool filterRendering = renderer->capabilities() & QgsFeatureRendererV2::Filter;

    if ( filterRendering )
    {
      // add attributes used for filtering
      foreach( const QString& attr, renderer->filterReferencedColumns() )
      {
        if ( !attrNames.contains(attr) )
        {
          attrNames << attr;
        }
      }
      if (!renderer->prepareFilter( context, vl->pendingFields() )) {
        std::cout << "PROBLEM preparing filter" << std::endl;
      }
    }

    foreach( const QString& attr, attrNames )
    {
      std::cout << "attribute: " << attr.toUtf8().constData() << std::endl;
    }

    QgsFeature fet;
    // a label layer has no CRS per se (it refers multiple layers), so we need to access labeling settings
    QgsPalLayerSettings& plyr = pal->layer( vl->id() );
    QgsRectangle dataExtent;
    if ( plyr.ct )
    {
      dataExtent = plyr.ct->transformBoundingBox( context.extent(), QgsCoordinateTransform::ReverseTransform );
    }
    else
    {
      dataExtent = context.extent();
    }
    QgsFeatureRequest req = QgsFeatureRequest().setFilterRect( dataExtent ).setSubsetOfAttributes( attrNames, vl->pendingFields() );

    QgsFeatureIterator fit = vl->getFeatures( req );
    while ( fit.nextFeature( fet ) )
    {
      if ( context.renderingStopped() )
      {
        cancelled = true;
        break;
      }

      // for symbol levels, test that this feature is actually drawn
      if ( filterRendering && ! renderer->willRenderFeature(fet) )
      {
        continue;
      }

      nothingToLabel = false;
      if ( hasLabels )
      {
        pal->registerFeature( vl->id(), fet, context );
      }

      // diagram features
      if ( hasDiagrams )
      {
        pal->registerDiagramFeature( vl->id(), fet, context );
      }
    }

    if ( context.renderingStopped() )
    {
      cancelled = true;
      break;
    }
  }

  if ( !cancelled && !nothingToLabel )
  {
    pal->drawLabeling( context );
    pal->exit();
  }

  if ( !imgPainter.isNull() )
  {
    // restore painter
    imgPainter->end();
    context.setPainter( oldPainter );
    oldPainter->drawImage( QPoint(0,0), *mCacheImage );
  }

  return !cancelled;
}

bool QgsLabelLayer::writeXml( QDomNode & layer_node,
                              QDomDocument & )
{
  // first get the layer element so that we can append the type attribute

  QDomElement mapLayerNode = layer_node.toElement();

  if ( mapLayerNode.isNull() || "maplayer" != mapLayerNode.nodeName() )
  {
    QgsMessageLog::logMessage( tr( "<maplayer> not found." ), tr( "Label" ) );
    return false;
  }

  mapLayerNode.setAttribute( "type", "label" );
  mapLayerNode.setAttribute( "cacheEnabled", mCacheEnabled ? "true" : "false" );

  return true;
}

bool QgsLabelLayer::readXml( const QDomNode & layer_node )
{
  // first get the layer element so that we can append the type attribute

  QDomElement mapLayerNode = layer_node.toElement();

  if ( mapLayerNode.isNull() || "maplayer" != mapLayerNode.nodeName() )
  {
    QgsMessageLog::logMessage( tr( "<maplayer> not found." ), tr( "Label" ) );
    return false;
  }

  mCacheEnabled = mapLayerNode.attribute("cacheEnabled") == "true";

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
