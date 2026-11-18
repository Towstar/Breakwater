import QtQuick
import QtQuick3D

Node {
    id: pad
    property string kind: "tank"
    property bool selected: false
    property bool detailed: true
    readonly property real length: kind === "treatment" ? 136 : kind === "trucking" || kind === "truck_yard" ? 130 : kind === "facility" ? 150 : 92
    readonly property real depth: kind === "facility" ? 116 : kind === "pump" ? 60 : 92
    PrincipledMaterial { id: concrete; baseColor: "#B7C1C1"; roughness: 0.96 }
    PrincipledMaterial { id: curb; baseColor: pad.selected ? "#E3EEF1" : "#A4B1B2"; roughness: 0.9 }
    PrincipledMaterial { id: paint; baseColor: pad.selected ? "#F0F5F4" : "#C7B77E"; roughness: 0.95 }
    Model { source: "#Cube"; y: -1; scale: Qt.vector3d(pad.length / 100, 0.04, pad.depth / 100); materials: concrete; castsShadows: false }
    Repeater3D {
        model: pad.detailed && pad.kind === "tank" ? [-1, 1] : []
        delegate: Node {
            required property int modelData
            Model { source: "#Cube"; y: 3; z: modelData * (pad.depth / 2 - 2); scale: Qt.vector3d(pad.length / 100, 0.06, 0.035); materials: curb }
            Model { source: "#Cube"; y: 3; x: modelData * (pad.length / 2 - 2); scale: Qt.vector3d(0.035, 0.06, pad.depth / 100); materials: curb }
        }
    }
    Model {
        source: "#Cube"; y: 1.5; z: pad.depth / 2 - 5
        scale: Qt.vector3d((pad.length - 12) / 100, 0.005, 0.016)
        materials: paint
    }
    Repeater3D {
        model: (pad.detailed || pad.selected) && pad.kind !== "tank" ? [-1, 1] : []
        delegate: Node {
            required property int modelData
            Model { source: "#Cube"; y: 1.2; z: modelData * (pad.depth / 2 - 7); scale: Qt.vector3d((pad.length - 14) / 100, 0.004, 0.015); materials: paint; castsShadows: false }
            Model { source: "#Cube"; y: 1.2; x: modelData * (pad.length / 2 - 7); scale: Qt.vector3d(0.015, 0.004, (pad.depth - 14) / 100); materials: paint; castsShadows: false }
        }
    }
}
