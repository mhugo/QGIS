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

QgsLabelLayerLegend::QgsLabelLayerLegend( QgsLabelLayer* layer ) : QgsMapLayerLegend(), mLayer(layer)
{
  connect( mLayer, SIGNAL( layersChanged() ), this, SIGNAL( itemsChanged() ) );
}

QList<QgsLayerTreeModelLegendNode*> QgsLabelLayerLegend::createLayerTreeModelLegendNodes( QgsLayerTreeLayer* nodeLayer )
{
  QList<QgsLayerTreeModelLegendNode*> nodes;

  foreach( QgsMapLayer* ml, QgsMapLayerRegistry::instance()->mapLayers() ) {
    if ( ml->type() != QgsMapLayer::VectorLayer ) {
      continue;
    }
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(ml);

    if ( mLayer->id() == vl->labelLayer() )
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

QgsLabelLayer::QgsLabelLayer( QString layerName )
  : QgsMapLayer( LabelLayer, layerName ),
    mInit( false )
{
  mLegend = new QgsLabelLayerLegend(this); // will be owned by QgsMapLayer
  setLegend( mLegend );

  mValid = true;

  connect( QgsMapLayerRegistry::instance(), SIGNAL( layersAdded(QList<QgsMapLayer*>) ), this, SLOT( onLayersAdded(QList<QgsMapLayer*>) ) );
  connect( QgsMapLayerRegistry::instance(), SIGNAL( layerWillBeRemoved(QString) ), this, SLOT( onLayerRemoved(QString) ) );
}

const QString QgsLabelLayer::MainLayerId = "_mainlabels_";

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
    connect( vl, SIGNAL( layerNameChanged() ), this, SLOT( onLayerRenamed() ) );
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
    disconnect( vl, SIGNAL( layerNameChanged() ), this, SLOT( onLayerRenamed() ) );
  }
  disconnect( QgsMapLayerRegistry::instance(), SIGNAL( layersAdded(QList<QgsMapLayer*>) ), this, SLOT( onLayersAdded(QList<QgsMapLayer*>) ) );
  disconnect( QgsMapLayerRegistry::instance(), SIGNAL( layerWillBeRemoved(QString) ), this, SLOT( onLayerRemoved(QString) ) );
}

bool QgsLabelLayer::addLayer( QgsVectorLayer* vl )
{
  if ( id() == vl->labelLayer() )
  {
    // make sure the list is always sorted and elements are unique
    // (loop needed to optimize both operations)
    auto insertionIt = mLayers.end();
    for ( auto it = mLayers.begin(); it != mLayers.end(); ++it )
    {
      int c = (*it)->id().compare( vl->id() );
      if ( c == 0 )
      {
        // already there, abort
        return true;
      }
      if ( c > 0 )
      {
        // we passed the point of insertion, insert it here
        insertionIt = it;
        break;
      }
    }
    mLayers.insert( insertionIt, vl );
    return true;
  }
  return false;
}

QList<QgsVectorLayer*> QgsLabelLayer::vectorLayers() const
{
  return mLayers;
}

void QgsLabelLayer::onLabelLayerChanged( const QString& oldLabelLayer )
{
  bool doUpdateLegend = false;

  QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(sender());
  // if the old label layer was this one
  // remove it from the list of layers
  if ( id() == oldLabelLayer )
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
    emit layersChanged();
  }
}

void QgsLabelLayer::onLayerRenamed()
{
  QgsVectorLayer* vl = static_cast<QgsVectorLayer*>( sender() );
  if ( id() == vl->labelLayer() )
  {
    emit layersChanged();
  }
}

void QgsLabelLayer::onLayersAdded( QList<QgsMapLayer*> layers )
{
  bool doEmit = false;
  foreach( QgsMapLayer* ml, layers )
  {
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
      doEmit = true;
    }

    // register to label layer change signal
    connect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
    connect( vl, SIGNAL( layerNameChanged() ), this, SLOT( onLayerRenamed() ) );
  }
  if ( doEmit )
  {
    emit layersChanged();
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
  if ( id() == vl->labelLayer() )
  {
    mLayers.removeOne( vl );
    // update after the actual layer removal
    QTimer::singleShot( 0, this, SIGNAL(layersChanged()) );
  }

  // unregister label change signal
  disconnect( vl, SIGNAL( labelLayerChanged(const QString&) ), this, SLOT( onLabelLayerChanged(const QString&) ) );
  disconnect( vl, SIGNAL( layerNameChanged() ), this, SLOT( onLayerRenamed() ) );
}

QgsLabelLayer* QgsLabelLayer::mainLabelLayer()
{
  static QgsLabelLayer mainLayer;
  static bool init = false;
  if (!init) {
    // this is a special layer, with a special id
    mainLayer.mID = MainLayerId;
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
  QgsPalLabeling* mainPal = dynamic_cast<QgsPalLabeling*>(context.labelingEngine());
  Q_ASSERT( mainPal );
  QScopedPointer<QgsPalLabeling> pal( static_cast<QgsPalLabeling*>(mainPal->clone()) );
  pal->setResults( mainPal->takeResults() );

  bool nothingToLabel = true;
  foreach( QgsVectorLayer* vl, mLayers )
  {
    // scale-based visibility test
    if ( vl->hasScaleBasedVisibility() && ( context.scaleFactor() < vl->minimumScale() || context.scaleFactor() > vl->maximumScale() ) )
    {
      continue;
    }

    bool hasLabels = false;
    bool hasDiagrams = false;
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

    if ( !hasLabels && !hasDiagrams )
    {
      continue;
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
      renderer->prepareFilter( context, vl->pendingFields() );
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
    pal->drawLabeling( context, /* retainPreviousResults = */ true );
    // set results as partial results for the next label layer
    mainPal->setResults( pal->takeResults() );
    pal->exit();
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

namespace QgsLabelLayerUtils
{

bool hasBlendModes( const QgsLabelLayer* layer )
{
  foreach( QgsVectorLayer *vl, layer->vectorLayers() )
  {
    QgsPalLayerSettings s;
    s.readFromLayer( vl );
    if ( s.blendMode || s.dataDefinedIsActive( QgsPalLayerSettings::FontBlendMode ) ||
         ((s.bufferDraw || s.dataDefinedIsActive( QgsPalLayerSettings::BufferDraw )) &&
          (s.bufferBlendMode || s.dataDefinedIsActive( QgsPalLayerSettings::BufferBlendMode ))) ||
         ((s.shapeDraw || s.dataDefinedIsActive( QgsPalLayerSettings::ShapeDraw )) &&
          (s.shapeBlendMode || s.dataDefinedIsActive( QgsPalLayerSettings::ShapeBlendMode ))) ||
         ((s.shadowDraw || s.dataDefinedIsActive( QgsPalLayerSettings::ShadowDraw )) &&
          (s.shadowBlendMode || s.dataDefinedIsActive( QgsPalLayerSettings::ShadowBlendMode ))) )
    {
      return true;
    }
  }
  return false;
}

}
