import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

// Schematic decoration only. It has no pick targets, travel times or hydraulic geometry.
Node {
    id: site
    property real centerX: 0
    property real centerZ: 0
    property real spanX: 1600
    property real spanZ: 1600
    property bool showLane: true
    property bool showGrid: false
    property int quality: 1
    readonly property real workWidth: Math.max(180, spanX - 140)
    readonly property real workDepth: Math.max(180, spanZ - (showLane ? 210 : 140))
    readonly property real workCenterZ: showLane ? 35 : 0
    readonly property real laneZ: -spanZ / 2 + 55
    readonly property real gridStep: 20 * Math.pow(2, Math.max(0, Math.ceil(Math.log2(Math.max(spanX, spanZ) / 1200))))
    readonly property real gridCenterX: Math.round(centerX / gridStep) * gridStep
    readonly property real gridCenterZ: Math.round((centerZ + workCenterZ) / gridStep) * gridStep
    readonly property int gridColumns: Math.min(65, Math.max(1, 2 * Math.floor((workWidth / 2 - Math.abs(gridCenterX - centerX) - 20) / gridStep) + 1))
    readonly property int gridRows: Math.min(65, Math.max(1, 2 * Math.floor((workDepth / 2 - Math.abs(gridCenterZ - centerZ - workCenterZ) - 20) / gridStep) + 1))
    position: Qt.vector3d(centerX, -3, centerZ)
    PrincipledMaterial {
        id: gravel
        baseColor: "#D8DEDC"; roughness: 1
        baseColorMap: Texture {
            source: "qrc:/breakwater/assets/site-gravel.svg"
            tilingModeHorizontal: Texture.Repeat; tilingModeVertical: Texture.Repeat
            scaleU: Math.max(1, site.spanX / 40); scaleV: Math.max(1, site.spanZ / 40)
            generateMipmaps: true; mipFilter: Texture.Linear
        }
    }
    PrincipledMaterial { id: concrete; baseColor: "#B6C1C3"; roughness: 0.97 }
    PrincipledMaterial { id: asphalt; baseColor: "#59666D"; roughness: 0.98 }
    PrincipledMaterial { id: edge; baseColor: "#B7C1BF"; roughness: 0.94 }
    PrincipledMaterial { id: paint; baseColor: "#C6BC89"; roughness: 0.95 }
    Model {
        source: "#Rectangle"; y: -2; eulerRotation.x: -90
        scale: Qt.vector3d(site.spanX * 1.65 / 100, site.spanZ * 1.65 / 100, 1)
        materials: gravel; castsShadows: false; pickable: false
    }
    Model {
        source: "#Cube"; y: -1; z: site.workCenterZ
        scale: Qt.vector3d(site.workWidth / 100, 0.02, site.workDepth / 100)
        materials: concrete; castsShadows: false; pickable: false
    }
    Repeater3D {
        model: [-1, 1]
        delegate: Node {
            required property int modelData
            Model {
                source: "#Cube"; y: 0.1; z: site.workCenterZ + modelData * (site.workDepth / 2 - 8)
                scale: Qt.vector3d((site.workWidth - 16) / 100, 0.002, 0.015)
                materials: paint; castsShadows: false; pickable: false
            }
            Model {
                source: "#Cube"; y: 0.1; x: modelData * (site.workWidth / 2 - 8); z: site.workCenterZ
                scale: Qt.vector3d(0.015, 0.002, (site.workDepth - 16) / 100)
                materials: paint; castsShadows: false; pickable: false
            }
        }
    }
    Model {
        objectName: "siteLayoutGrid"
        visible: site.showGrid && site.gridColumns > 1 && site.gridRows > 1
        // The editor's pointer projection uses world Y=0. Keep the guide on
        // that plane, above the paving, rather than fighting its depth surface.
        position: Qt.vector3d(site.gridCenterX - site.centerX, 3, site.gridCenterZ - site.centerZ)
        eulerRotation.x: -90
        geometry: GridGeometry { horizontalLines: site.gridRows; verticalLines: site.gridColumns; horizontalStep: site.gridStep; verticalStep: site.gridStep }
        materials: DefaultMaterial { lighting: DefaultMaterial.NoLighting; diffuseColor: "#667C86"; opacity: 0.22 }
        castsShadows: false; receivesShadows: false; pickable: false
    }
    Model {
        visible: site.showLane; source: "#Cube"; y: -1.4; z: site.laneZ
        scale: Qt.vector3d((site.spanX - 100) / 100, 0.012, 0.72)
        castsShadows: false; pickable: false; materials: asphalt
    }
    Repeater3D {
        model: site.showLane ? [-1, 1] : []
        delegate: Node {
            required property int modelData
            Model {
                source: "#Cube"; y: -1.15; z: site.laneZ + modelData * 37.5
                scale: Qt.vector3d((site.spanX - 100) / 100, 0.017, 0.03)
                materials: edge; castsShadows: false; pickable: false
            }
            Model {
                source: "#Cube"; y: -0.72; z: site.laneZ + modelData * 31
                scale: Qt.vector3d((site.spanX - 110) / 100, 0.002, 0.012)
                materials: paint; castsShadows: false; pickable: false
            }
        }
    }
    Repeater3D {
        model: site.showLane && site.quality > 0 ? 12 : 0
        delegate: Model {
            required property int index
            source: "#Cube"; y: -0.72; z: site.laneZ
            x: (index - 5.5) * (site.spanX - 160) / 12
            scale: Qt.vector3d((site.spanX - 160) / 2400, 0.002, 0.014)
            castsShadows: false; pickable: false; materials: paint
        }
    }
}
