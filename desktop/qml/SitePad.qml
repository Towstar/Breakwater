import QtQuick
import QtQuick3D

Node {
    id: pad
    property string kind: "tank"
    property bool selected: false
    readonly property real length: kind === "treatment" || kind === "trucking" || kind === "truck_yard" ? 130 : kind === "facility" ? 150 : 88
    readonly property real depth: kind === "facility" ? 116 : 88
    PrincipledMaterial { id: concrete; baseColor: "#B9B9AF"; roughness: 0.96 }
    PrincipledMaterial { id: curb; baseColor: pad.selected ? "#DCEEF4" : "#CAC9BE"; roughness: 0.9 }
    Model { source: "#Cube"; y: -1; scale: Qt.vector3d(pad.length / 100, 0.04, pad.depth / 100); materials: concrete; castsShadows: false }
    Repeater3D {
        model: pad.kind === "tank" ? [-1, 1] : []
        delegate: Node {
            required property int modelData
            Model { source: "#Cube"; y: 3; z: modelData * (pad.depth / 2 - 2); scale: Qt.vector3d(pad.length / 100, 0.06, 0.035); materials: curb }
            Model { source: "#Cube"; y: 3; x: modelData * (pad.length / 2 - 2); scale: Qt.vector3d(0.035, 0.06, pad.depth / 100); materials: curb }
        }
    }
    Model {
        source: "#Cube"; y: 1.5; z: pad.depth / 2 - 5
        scale: Qt.vector3d((pad.length - 12) / 100, 0.005, 0.016)
        materials: PrincipledMaterial { baseColor: pad.selected ? "#3D758B" : "#DAD4B5"; roughness: 1 }
    }
}
