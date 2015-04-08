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
