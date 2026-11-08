pragma ComponentBehavior: Bound
import QtQuick
import QtQuick3D

// Presentation-only assembly. Its Y axis follows the caller's existing route
// transform; neither its radius nor its flange spacing participates in physics.
Node {
    id: pipe
    required property string pickName
    required property real span
    property color tint: "#8f9da6"
    property real diameter: 6.2
    property bool fittings: true
    readonly property real joint: Math.max(0, span / 2 - Math.min(7, span * 0.12))

    PrincipledMaterial { id: metal; baseColor: pipe.tint; metalness: 0.42; roughness: 0.48 }
    PrincipledMaterial { id: flange; baseColor: "#bac2c8"; metalness: 0.5; roughness: 0.48 }
    PrincipledMaterial { id: gasket; baseColor: "#303940"; metalness: 0.12; roughness: 0.78 }
    Model {
        objectName: pipe.pickName; source: "#Cylinder"; pickable: true
        visible: pipe.span > 0.01
        scale: Qt.vector3d(pipe.diameter / 100, pipe.span / 100, pipe.diameter / 100)
        materials: metal
    }
    Repeater3D {
        model: pipe.fittings && pipe.span > 32 ? [-1, 1] : []
        delegate: Node {
            required property var modelData
            y: modelData * pipe.joint
            Model {
                objectName: pipe.pickName; source: "#Cylinder"; pickable: true
                scale: Qt.vector3d(pipe.diameter * 1.85 / 100, 0.024, pipe.diameter * 1.85 / 100)
                materials: flange
            }
            Model {
                source: "#Cylinder"; y: 1.5
                scale: Qt.vector3d(pipe.diameter * 1.52 / 100, 0.008, pipe.diameter * 1.52 / 100)
                materials: gasket
            }
            // Two visible bolt heads per joint, rather than an unbounded ring.
            Repeater3D {
                model: [-1, 1]
                delegate: Model {
                    required property var modelData
                    source: "#Cylinder"; x: modelData * pipe.diameter * 0.65
                    scale: Qt.vector3d(0.014, 0.038, 0.014)
                    materials: gasket
                }
            }
        }
    }
}
