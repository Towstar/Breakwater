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
    // Keep the schematic joint clear of a typical equipment shell. This offset
    // is visual only and never changes the route endpoints or physical length.
    readonly property real joint: Math.max(0, span / 2 - Math.min(34, span * 0.22))

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
                source: "#Cylinder"
                scale: Qt.vector3d(pipe.diameter * 1.62 / 100, 0.008, pipe.diameter * 1.62 / 100)
                materials: gasket
            }
            Repeater3D {
                model: [-1, 1]
                delegate: Model {
                    required property var modelData
                    source: "#Cylinder"; y: modelData * 1.2
                    scale: Qt.vector3d(pipe.diameter * 1.85 / 100, 0.016, pipe.diameter * 1.85 / 100)
                    materials: flange
                }
            }
            // Four bounded through-bolts make a paired joint readable up close.
            // The tube retains picking; none of the joint decoration intercepts it.
            Repeater3D {
                model: [0, 90, 180, 270]
                delegate: Model {
                    required property real modelData
                    source: "#Cylinder"
                    x: Math.cos(modelData * Math.PI / 180) * pipe.diameter * 0.72
                    z: Math.sin(modelData * Math.PI / 180) * pipe.diameter * 0.72
                    scale: Qt.vector3d(0.018, 0.052, 0.018)
                    materials: gasket
                }
            }
        }
    }
}
