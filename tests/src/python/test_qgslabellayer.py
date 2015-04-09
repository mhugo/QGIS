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
from utilities import getQgisTestApp, unittest, renderMapToImage, unitTestDataPath

from PyQt4.QtCore import *
from qgis.core import *
from qgis.core import qgsfunction, register_function

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
    s.writeToLayer( vl )

class TestPyQgsLabelLayer(unittest.TestCase):

    def testCache(self):
        self.TEST_DATA_DIR = unitTestDataPath()
        fi = QFileInfo( self.TEST_DATA_DIR + "/france_parts.shp")
        vl = QgsVectorLayer( fi.filePath(), fi.completeBaseName(), "ogr" )
        assert vl.isValid()
        vl2 = QgsVectorLayer( fi.filePath(), fi.completeBaseName(), "ogr" )
        assert vl2.isValid()

        r = QgsSingleSymbolRendererV2( QgsSymbolV2.defaultSymbol( QGis.Polygon ) )
        vl.setRendererV2( r )
        vl2.setRendererV2( r )

        # enable labels
        enableLabels( vl, "NAME_1 || markRendering(0)", True )
        enableLabels( vl2, "NAME_1 || markRendering(0)", True )

        ll = QgsLabelLayer()
        QgsMapLayerRegistry.instance().addMapLayers([vl, vl2, ll])

        # map settings
        ms = QgsMapSettings()
        crs = QgsCoordinateReferenceSystem(2154)
        ms.setDestinationCrs( crs )
        ms.setOutputSize( QSize(420, 280) )
        ms.setOutputDpi(72)
        ms.setFlag(QgsMapSettings.Antialiasing, True)
        ms.setFlag(QgsMapSettings.UseAdvancedEffects, False)
        ms.setFlag(QgsMapSettings.ForceVectorOutput, False)  # no caching?
        ms.setDestinationCrs(crs)
        ms.setCrsTransformEnabled(True)
        ms.setMapUnits(crs.mapUnits())  # meters
        ms.setExtent( ms.layerExtentToOutputExtent( vl, vl.extent() ) )

        ms.setFlags( ms.flags() | QgsMapSettings.DrawLabeling );

        ms.setLayers([ll.id(), vl.id() ])

        def _testCache():
            global gRendered
            # check that it is rendered
            gRendered = 0
            renderMapToImage(ms)
            assert gRendered == 8
            # then use the cache
            gRendered = 0
            renderMapToImage(ms)
            assert gRendered == 0

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

    def xtest1(self):
        self.TEST_DATA_DIR = unitTestDataPath()
        fi = QFileInfo( self.TEST_DATA_DIR + "/france_parts.shp")
        vl = QgsVectorLayer( fi.filePath(), fi.completeBaseName(), "ogr" )
        assert vl.isValid()

        r = QgsSingleSymbolRendererV2( QgsSymbolV2.defaultSymbol( QGis.Polygon ) )
        vl.setRendererV2( r )

        # enable labels
        enableLabels( vl, "NAME_1" )

        # second layer, different CRS
        vl2 = QgsVectorLayer( "/home/hme/Bureau/test_vlayer/departements.shp", "regions", "ogr" )
        assert vl2.isValid()
        enableLabels( vl2, "NOM_DEPT" )

        ll = QgsLabelLayer()
        ll2 = QgsLabelLayer( "labels2" )
        QgsMapLayerRegistry.instance().addMapLayers([vl, vl2, ll, ll2])

        # map settings
        ms = QgsMapSettings()
        crs = QgsCoordinateReferenceSystem(2154)
        ms.setDestinationCrs( crs )
        ms.setOutputSize( QSize(420, 280) )
        ms.setOutputDpi(72)
        ms.setFlag(QgsMapSettings.Antialiasing, True)
        ms.setFlag(QgsMapSettings.UseAdvancedEffects, False)
        ms.setFlag(QgsMapSettings.ForceVectorOutput, False)  # no caching?
        ms.setDestinationCrs(crs)
        ms.setCrsTransformEnabled(True)
        ms.setMapUnits(crs.mapUnits())  # meters
        ms.setExtent( ms.layerExtentToOutputExtent( vl, vl.extent() ) )

        ms.setFlags( ms.flags() | QgsMapSettings.DrawLabeling );

        if False:
            ms.setLayers([ll.id(), ll2.id(), vl2.id(), vl.id() ])

            img = renderMapToImage( ms )
            img.save("t1.png")

        vl2.setLabelLayer("labels2")
        ms.setLayers([ll2.id(), ll.id(), vl2.id(), vl.id() ])

        img = renderMapToImage( ms )
        img.save("t2.png")

if __name__ == '__main__':
    unittest.main()
