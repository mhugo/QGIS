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

  mLayers = layerIds;
  mInvalidated = false;

  return hit;
}

void QgsLabelLayerCacheTest::invalidate()
{
  mInvalidated = true;
}

QgsLabelLayerLegend::QgsLabelLayerLegend( QgsLabelLayer* layer ) : QgsMapLayerLegend(), mLayer(layer)
{
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

void QgsLabelLayerLegend::emitItemsChanged()
{
  emit itemsChanged();
}

QgsLabelLayer::QgsLabelLayer( QString layerName )
  : QgsMapLayer( LabelLayer, layerName ),
    mInit( false ),
    mCacheEnabled( false )
{
  mLegend = new QgsLabelLayerLegend(this); // will be own by QgsMapLayer
  setLegend( mLegend );

  mValid = true;

  connect( QgsMapLayerRegistry::instance(), SIGNAL( layersAdded(QList<QgsMapLayer*>) ), this, SLOT( onLayersAdded(QList<QgsMapLayer*>) ) );
  connect( QgsMapLayerRegistry::instance(), SIGNAL( layerWillBeRemoved(QString) ), this, SLOT( onLayerRemoved(QString) ) );

  // invalidate cache when triggerRepaint is called, to be consistent with QgsMapLayer's behaviour
  connect( this, SIGNAL( repaintRequested() ), this, SIGNAL( invalidateCache() ) );
}

void QgsLabelLayer::init()
{
  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() )
  {
    if ( ml->type() != QgsMapLayer::VectorLayer )
    {
      continue;
    }
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);
    addLayer( vl );

    // register to label layer change signal
    connect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
    // register to repaint request
    connect( vl, SIGNAL( repaintRequested() ), this, SLOT( onRepaintLayer() ) );
  }

  // now we are initialized
  mInit = true;
}

QgsLabelLayer::~QgsLabelLayer()
{
  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() )
  {
    if ( ml->type() != QgsMapLayer::VectorLayer )
    {
      continue;
    }
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);
    disconnect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
    disconnect( vl, SIGNAL( repaintRequested() ), this, SLOT( onRepaintLayer() ) );
  }
  disconnect( QgsMapLayerRegistry::instance(), SIGNAL( layersAdded(QList<QgsMapLayer*>) ), this, SLOT( onLayersAdded(QList<QgsMapLayer*>) ) );
  disconnect( QgsMapLayerRegistry::instance(), SIGNAL( layerWillBeRemoved(QString) ), this, SLOT( onLayerRemoved(QString) ) );
}

bool QgsLabelLayer::addLayer( QgsVectorLayer* vl )
{
  std::cout << "addLayer with label layer " << vl->labelLayer().toUtf8().constData() << std::endl;
  if ( (vl->labelLayer().isEmpty() && id() == "_mainlabels_") ||
       (!vl->labelLayer().isEmpty() && id() == vl->labelLayer() ) )
  {
    if ( !mLayers.contains(vl) )
    {
      mLayers << vl;
      return true;
    }
  }
  return false;
}

void QgsLabelLayer::onLabelLayerChanged( const QString& oldLabelLayer )
{
  bool doUpdateLegend = false;

  QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(sender());
  std::cout << id().toUtf8().constData() << " label laye changed from " << oldLabelLayer.toUtf8().constData() << " to " << vl->labelLayer().toUtf8().constData() << std::endl;
  // if the old label layer was this one
  // remove it from the list of layers
  if ( (oldLabelLayer.isEmpty() && id() == "_mainlabels_") ||
       (!oldLabelLayer.isEmpty() && id() == oldLabelLayer) )
  {
    doUpdateLegend = true;
    mLayers.removeOne( vl );
  }

  // if the new label layer is this one
  // add it to the list of layers
  if ( addLayer( vl ) )
  {
    doUpdateLegend = true;
  }
  if ( doUpdateLegend )
  {
    updateLegend();
  }
}

void QgsLabelLayer::onLayersAdded( QList<QgsMapLayer*> layers )
{
  foreach( QgsMapLayer* ml, layers )
  {
    std::cout << "onLayersAdded " << ml->id().toUtf8().constData() << std::endl;
    if ( ml == this )
    {
      // we must wait that the label layer is added to the registry to have its final ID
      // so that we can know which other vector layers refers to it
      init();
      continue;
    }

    // if not yet initialized
    if ( !mInit )
    {
      continue;
    }

    if ( ml->type() != QgsMapLayer::VectorLayer )
    {
      continue;
    }

    QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(ml);
    if ( addLayer( vl ) )
    {
      updateLegend();
    }

    // register to label layer change signal
    connect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
    // register to repaint request
    connect( vl, SIGNAL( repaintRequested() ), this, SLOT( onRepaintLayer() ) );
  }
}

void QgsLabelLayer::onLayerRemoved( QString layerid )
{
  QgsMapLayer* ml = QgsMapLayerRegistry::instance()->mapLayer(layerid);
  if ( !ml )
  {
    return;
  }

  if ( ml->type() != QgsMapLayer::VectorLayer )
  {
    return;
  }

  QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(ml);
  if ( (vl->labelLayer().isEmpty() && id() == "_mainlabels_") ||
       (!vl->labelLayer().isEmpty() && id() == vl->labelLayer() ) )
  {
    mLayers.removeOne( vl );
    updateLegend();
  }

  // unregister label change signal
  disconnect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
  // unregister repaint request
  disconnect( vl, SIGNAL( repaintRequested() ), this, SLOT( onRepaintLayer() ) );
}

void QgsLabelLayer::onRepaintLayer()
{
  QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(sender());
  std::cout << vl->id().toUtf8().constData() << " asks for repaint" << std::endl;
  // if this is one of referenced layers, invalidate cache
  if ( mLayers.contains( vl ) ) {
    invalidateCache();
  }
}

void QgsLabelLayer::updateLegend()
{
  // if the legend is mine
  // update it
  if ( legend() == mLegend )
  {
    std::cout << "update legend" << std::endl;
    mLegend->emitItemsChanged();
  }
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
    mainLayer.init();
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
  std::cout << "begin draw" << std::endl;
  foreach( QgsVectorLayer* vl, mLayers )
  {
    std::cout << vl->id().toUtf8().constData() << std::endl;
    if ( pal->willUseLayer( vl ) || (vl->diagramRenderer() && vl->diagramLayerSettings()) )
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
