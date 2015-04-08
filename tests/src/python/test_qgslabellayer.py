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

QGISAPP, CANVAS, IFACE, PARENT = getQgisTestApp()


def enableLabels( vl, attribute ):
    s = QgsPalLayerSettings()
    s.readFromLayer( vl )
    s.enabled = True
    s.fieldName = attribute
    s.writeToLayer( vl )

class TestPyQgsLabelLayer(unittest.TestCase):

    def test1(self):
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
