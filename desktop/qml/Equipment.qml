import QtQuick
import QtQuick3D

Node {
    id: equipment
    required property var assetData
    property var draggedIds: []
    property vector3d dragDelta: Qt.vector3d(0, 0, 0)
    property bool detailed: true
    property int connectionState: 0
    // The editor walks to this exact outer node when beginning a drag.
    objectName: "equipment:" + assetData.id
    position: Qt.vector3d(assetData.x + (draggedIds.indexOf(assetData.id) >= 0 ? dragDelta.x : 0), 0, assetData.z + (draggedIds.indexOf(assetData.id) >= 0 ? dragDelta.z : 0))
    property color equipmentColor: assetData.violated ? "#eb8585" : assetData.selected ? "#edf1f3" : "#a6b0b8"

    SitePad { visible: equipment.detailed || equipment.assetData.selected; detailed: equipment.detailed; kind: equipment.assetData.kind; selected: equipment.assetData.selected }
    IndustrialEquipment {
        kind: equipment.assetData.kind
        pickName: "asset:" + equipment.assetData.id
        selected: equipment.assetData.selected
        detailed: equipment.detailed
        showMarker: equipment.detailed || equipment.assetData.selected || equipment.connectionState !== 0
        connectionState: equipment.connectionState
        abnormal: equipment.assetData.violated
        // Controller display fill only. Raw volume and capacity remain in the
        // inspector/charts and are never modified by the visual assembly.
        fillFraction: equipment.assetData.fill
    }
}
