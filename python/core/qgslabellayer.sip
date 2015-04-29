
class QgsLabelLayer : QgsMapLayer
{
%TypeHeaderCode
#include "qgslabellayer.h"
%End

  public:
    QgsLabelLayer( QString layerName = "" );

    /**
     * Rendering part
     */
    virtual bool draw( QgsRenderContext& rendererContext );

    virtual QgsMapLayerRenderer* createMapRenderer( QgsRenderContext& rendererContext );

    virtual bool readSymbology( const QDomNode& node, QString& errorMessage);

    virtual bool writeSymbology( QDomNode& node, QDomDocument& doc, QString& errorMessage) const;

    QList<QgsVectorLayer*> vectorLayers() const;

    static QgsLabelLayer* mainLabelLayer();
};
