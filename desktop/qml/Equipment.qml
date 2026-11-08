import QtQuick
import QtQuick3D

Node {
    id: equipment
    required property var assetData
    // The editor walks to this exact outer node when beginning a drag.
    objectName: "equipment:" + assetData.id
    position: Qt.vector3d(assetData.x, 0, assetData.z)
    property color equipmentColor: assetData.violated ? "#eb8585" : assetData.selected ? "#edf1f3" : "#a6b0b8"

    IndustrialEquipment {
        kind: equipment.assetData.kind
        pickName: "asset:" + equipment.assetData.id
        selected: equipment.assetData.selected
        abnormal: equipment.assetData.violated
        // Controller display fill only. Raw volume and capacity remain in the
        // inspector/charts and are never modified by the visual assembly.
        fillFraction: equipment.assetData.fill
    }
}
