import QtQuick
import QtQuick3D

// Schematic decoration only. It has no pick targets, travel times or hydraulic geometry.
Node {
    id: site
    property real centerX: 0
    property real centerZ: 0
    property real spanX: 1600
    property real spanZ: 1600
    property bool showLane: true
    position: Qt.vector3d(centerX, -4, centerZ)
    PrincipledMaterial {
        id: gravel
        baseColor: "#B4B4A9"; roughness: 1
        baseColorMap: Texture {
            source: "qrc:/breakwater/assets/site-gravel.svg"
            tilingModeHorizontal: Texture.Repeat; tilingModeVertical: Texture.Repeat
            scaleU: Math.max(1, site.spanX / 64); scaleV: Math.max(1, site.spanZ / 64)
            generateMipmaps: true; mipFilter: Texture.Linear
        }
    }
    Model { source: "#Rectangle"; eulerRotation.x: -90; scale: Qt.vector3d(site.spanX / 100, site.spanZ / 100, 1); materials: gravel; castsShadows: false }
    Model {
        visible: site.showLane; source: "#Cube"; y: 0.8; z: -site.spanZ * 0.34
        scale: Qt.vector3d(site.spanX * 0.85 / 100, 0.012, 0.85)
        castsShadows: false
        materials: PrincipledMaterial { baseColor: "#5B6161"; roughness: 0.98 }
    }
    Repeater3D {
        model: site.showLane ? 12 : 0
        delegate: Model {
            required property int index
            source: "#Cube"; y: 1.8; z: -site.spanZ * 0.34
            x: (index - 5.5) * site.spanX * 0.065
            scale: Qt.vector3d(site.spanX * 0.028 / 100, 0.004, 0.025)
            castsShadows: false
            materials: PrincipledMaterial { baseColor: "#D6D2BE"; roughness: 1 }
        }
    }
}
