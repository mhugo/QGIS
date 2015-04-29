"""QGIS Unit tests for QgsLabelLayer

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Hugo Mercier (hugo dot mercier at oslandia dot com)'
__date__ = '02/04/2015'
__copyright__ = 'Copyright 2015, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis
from utilities import getQgisTestApp, unittest, renderMapToImage, unitTestDataPath, getTestFont

from PyQt4.QtCore import *
from PyQt4.QtGui import QPainter, QColor
from qgis.core import *
from qgis.core import qgsfunction, register_function

import os

QGISAPP, CANVAS, IFACE, PARENT = getQgisTestApp()

gRendered = 0

# custom expression function that has side effect
@qgsfunction(args="auto", group="Custom")
def markRendering(value1, feature, parent):
    global gRendered
    gRendered += 1
    return "*"

def enableLabels( vl, attribute, isExpr = False ):
    s = QgsPalLayerSettings()
    s.readFromLayer( vl )
    s.enabled = True
    s.fieldName = attribute
    s.isExpression = isExpr
    s.textFont = getTestFont()
    s.writeToLayer( vl )

def renderToImage( mapsettings, cache = None ):
    job = QgsMapRendererSequentialJob(mapsettings)
    if cache is not None:
        job.setCache( cache )
    job.start()
    job.waitForFinished()

    return job.renderedImage()

class TestPyQgsLabelLayer(unittest.TestCase):

    def setUp(self):
        # default mapsettings
        self.ms = QgsMapSettings()
        crs = QgsCoordinateReferenceSystem(2154)
        self.ms.setDestinationCrs( crs )
        self.ms.setOutputSize( QSize(420, 280) )
        self.ms.setOutputDpi(72)
        self.ms.setFlag(QgsMapSettings.Antialiasing, True)
        self.ms.setFlag(QgsMapSettings.UseAdvancedEffects, False)
        self.ms.setFlag(QgsMapSettings.ForceVectorOutput, False)  # no caching?
        self.ms.setDestinationCrs(crs)
        self.ms.setMapUnits(crs.mapUnits())  # meters
        self.ms.setFlags( self.ms.flags() | QgsMapSettings.DrawLabeling | QgsMapSettings.UseAdvancedEffects );

        self.TEST_DATA_DIR = unitTestDataPath()

    def tearDown( self ):
        QgsMapLayerRegistry.instance().removeAllMapLayers()

    def xtestCache(self):
        fi = QFileInfo( os.path.join(self.TEST_DATA_DIR,"france_parts.shp") )
        vl = QgsVectorLayer( fi.filePath(), fi.completeBaseName(), "ogr" )
        assert vl.isValid()
        vl2 = QgsVectorLayer( fi.filePath(), fi.completeBaseName(), "ogr" )
        assert vl2.isValid()

        # enable labels with side effects
        enableLabels( vl, "NAME_1 || markRendering(0)", True )
        enableLabels( vl2, "NAME_1 || markRendering(0)", True )

        QgsMapLayerRegistry.instance().addMapLayers([vl, vl2], False)

        # map settings
        ms = self.ms
        ms.setExtent( ms.layerExtentToOutputExtent( vl, vl.extent() ) )
        ms.setLayers([vl.id(), vl2.id()])

        def _testCache():
            global gRendered
            cache = QgsMapRendererCache()
            # check that it is rendered
            gRendered = 0
            renderToImage(ms, cache)
            assert gRendered == 8
            # then use the cache
            gRendered = 0
            renderToImage(ms, cache)
            assert gRendered == 0

        def _testNoCache():
            global gRendered
            cache = QgsMapRendererCache()
            # check that it is rendered
            gRendered = 0
            renderToImage(ms, cache)
            assert gRendered == 8
            # then use the cache
            gRendered = 0
            renderToImage(ms, cache)
            assert gRendered == 8

        _testCache()

        # change the extent
        r = vl.extent()
        r.scale( 0.5 )
        ms.setExtent( ms.layerExtentToOutputExtent( vl, r ) )
        _testCache()

        # change the scale (changed by outputDPI)
        ms.setOutputDpi(90)
        _testCache()

        # change label layer reference
        vl.setLabelLayer( "invalid label layer" )
        global gRendered
        # check that it is rendered
        gRendered = 0
        renderMapToImage(ms)
        assert gRendered == 4
        vl.setLabelLayer("")

        # ask for repaint, this will invalidate the cache
        vl.triggerRepaint()
        _testCache()

        # enable some labeling blend modes
        # this should make the cache unusable
        s = QgsPalLayerSettings()
        s.readFromLayer( vl )

        s2 = QgsPalLayerSettings(s)
        s2.blendMode = QPainter.CompositionMode_Multiply
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.bufferDraw = True
        s2.bufferBlendMode = QPainter.CompositionMode_Multiply
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.shapeDraw = True
        s2.shapeBlendMode = QPainter.CompositionMode_Multiply
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.shadowDraw = True
        s2.shadowBlendMode = QPainter.CompositionMode_Multiply
        s2.writeToLayer( vl )
        _testNoCache()

        # data defined blend modes
        s2 = QgsPalLayerSettings(s)
        s2.setDataDefinedProperty(QgsPalLayerSettings.FontBlendMode, True, True, "1", "" )
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.bufferDraw = True
        s2.setDataDefinedProperty(QgsPalLayerSettings.BufferBlendMode, True, True, "1", "" )
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.setDataDefinedProperty(QgsPalLayerSettings.ShapeDraw, True, True, "1", "" )
        s2.setDataDefinedProperty(QgsPalLayerSettings.ShapeBlendMode, True, True, "1", "" )
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.setDataDefinedProperty(QgsPalLayerSettings.ShadowDraw, True, True, "1", "" )
        s2.setDataDefinedProperty(QgsPalLayerSettings.ShadowBlendMode, True, True, "1", "" )
        s2.writeToLayer( vl )
        _testNoCache()

        s2 = QgsPalLayerSettings(s)
        s2.writeToLayer( vl )
        _testCache()

    def testTwoLabelLayers(self):
        fi = QFileInfo( os.path.join(self.TEST_DATA_DIR,"france_parts.shp"))
        vl = QgsVectorLayer( fi.filePath(), fi.completeBaseName(), "ogr" )
        assert vl.isValid()

        # simple polygon renderer
        props = { "color": "0,127,0" }
        fillSymbol = QgsFillSymbolV2.createSimple( props )
        renderer = QgsSingleSymbolRendererV2( fillSymbol )
        vl.setRendererV2( renderer )

        # enable labels
        enableLabels( vl, "NAME_1" )

        # second layer, different CRS
        vl2 = QgsVectorLayer( "Point?crs=epsg:2154", "point_layer", "memory" )
        assert vl2.isValid()
        props = dict( name = 'circle', color = '255,0,0', size='4' )
        renderer = QgsSingleSymbolRendererV2( QgsMarkerSymbolV2.createSimple( props ) )
        vl2.setRendererV2( renderer )

        pr = vl2.dataProvider()

        # Enter editing mode
        vl2.startEditing()
        # add fields
        pr.addAttributes( [ QgsField("text", QVariant.String) ] )
        # add a feature
        fet = QgsFeature()
        c = QgsPoint(389873,6717319)
        fet.setGeometry( QgsGeometry.fromPoint(c) )
        fet.setFields( pr.fields(), True )
        fet.setAttribute( "text", "Other label" )
        fet.setFeatureId(1)
        pr.addFeatures( [ fet ] )
        # Commit changes
        vl2.commitChanges()

        enableLabels( vl2, "text" )

        ll = QgsLabelLayer( "labels" )
        QgsMapLayerRegistry.instance().addMapLayers([vl, vl2, ll])

        # map settings
        ms = self.ms
        ms.setCrsTransformEnabled(True)
        ms.setExtent( ms.layerExtentToOutputExtent( vl, vl.extent() ) )

        # render with all the layers in the main label layer
        ms.setLayers([vl2.id(), vl.id()])

        chk = QgsMultiRenderChecker()
        chk.setControlName( 'expected_labellayers1' )
        chk.setMapSettings( ms )
        res = chk.runTest( "labellayers1" )
        assert res

        # render with the point layer in its label layer
        vl2.setLabelLayer(ll.id())
        ms.setLayers([ll.id(), vl2.id(), vl.id()])

        chk = QgsMultiRenderChecker()
        chk.setControlName( 'expected_labellayers2' )
        chk.setMapSettings( ms )
        res = chk.runTest( "labellayers2" )
        assert res

        # render with the two layers in the same label layer
        vl.setLabelLayer(ll.id())
        ms.setLayers([ll.id(), vl2.id(), vl.id()])

        chk = QgsMultiRenderChecker()
        chk.setControlName( 'expected_labellayers1' )
        chk.setMapSettings( ms )
        res = chk.runTest( "labellayers1" )
        assert res

        ###############
        # labeling blend modes
        ###############
        s = QgsPalLayerSettings()
        s.readFromLayer( vl )

        s2 = QgsPalLayerSettings(s)
        s2.textColor = QColor("red")
        s2.blendMode = QPainter.CompositionMode_SoftLight
        s2.writeToLayer( vl )
        chk = QgsMultiRenderChecker()
        chk.setControlName( 'expected_labellayers3' )
        chk.setMapSettings( ms )
        res = chk.runTest( "labellayers3" )
        assert res

        # enable label layer blend mode
        # if there are labeling blend modes, the layer blend mode
        # is not used
        ll.setBlendMode( QPainter.CompositionMode_Difference )
        chk = QgsMultiRenderChecker()
        chk.setControlName( 'expected_labellayers3' )
        chk.setMapSettings( ms )
        res = chk.runTest( "labellayers3" )
        assert res

        s2 = QgsPalLayerSettings(s)
        s2.blendMode = QPainter.CompositionMode_SourceOver
        s2.writeToLayer( vl )
        chk = QgsMultiRenderChecker()
        chk.setControlName( 'expected_labellayers4' )
        chk.setMapSettings( ms )
        res = chk.runTest( "labellayers4" )
        assert res

if __name__ == '__main__':
    unittest.main()
