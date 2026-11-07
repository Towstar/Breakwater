import QtQuick
import QtQuick3D

Node {
    id: equipment
    required property var assetData
    objectName: "equipment:" + assetData.id
    position: Qt.vector3d(assetData.x, 0, assetData.z)
    property color equipmentColor: assetData.selected ? "#ffcf70" : (assetData.violated ? "#f16d72" : "#7893a6")

    Model {
        visible: equipment.assetData.kind === "tank"
        objectName: "asset:" + equipment.assetData.id
        source: "#Cylinder"
        position: Qt.vector3d(0, 35, 0)
        scale: Qt.vector3d(0.5, 0.7, 0.5)
        pickable: true
        materials: PrincipledMaterial { baseColor: equipment.equipmentColor; roughness: 0.8 }
    }
    Model {
        visible: equipment.assetData.kind === "tank" && equipment.assetData.fill > 0
        objectName: "asset:" + equipment.assetData.id
        source: "#Cylinder"
        position: Qt.vector3d(0, 35 * equipment.assetData.fill, 0)
        scale: Qt.vector3d(0.51, 0.7 * equipment.assetData.fill, 0.51)
        pickable: true
        materials: PrincipledMaterial { baseColor: equipment.assetData.violated ? "#ed6869" : "#35baca"; roughness: 0.65 }
    }
    Model {
        visible: equipment.assetData.kind === "source"
        objectName: "asset:" + equipment.assetData.id
        source: "#Cone"
        scale: Qt.vector3d(0.45, 0.95, 0.45)
        pickable: true
        materials: PrincipledMaterial { baseColor: equipment.assetData.selected ? "#ffcf70" : "#ce9c61"; roughness: 0.9 }
    }
    Model {
        visible: equipment.assetData.kind === "treatment"
        objectName: "asset:" + equipment.assetData.id
        source: "#Cube"
        position: Qt.vector3d(0, 20, 0)
        scale: Qt.vector3d(0.9, 0.4, 0.55)
        pickable: true
        materials: PrincipledMaterial { baseColor: equipment.assetData.selected ? "#ffcf70" : "#83b59d"; roughness: 0.8 }
    }
    Model {
        visible: equipment.assetData.kind === "trucking"
        objectName: "asset:" + equipment.assetData.id
        source: "#Cylinder"
        position: Qt.vector3d(0, 26, 0)
        eulerRotation.z: 90
        scale: Qt.vector3d(0.32, 0.8, 0.32)
        pickable: true
        materials: PrincipledMaterial { baseColor: equipment.equipmentColor; roughness: 0.6 }
    }
    Model {
        visible: equipment.assetData.kind === "trucking"
        objectName: "asset:" + equipment.assetData.id
        source: "#Cube"
        position: Qt.vector3d(51, 20, 0)
        scale: Qt.vector3d(0.25, 0.32, 0.33)
        pickable: true
        materials: PrincipledMaterial { baseColor: "#d99a6a"; roughness: 0.8 }
    }
    Model {
        source: "#Cylinder"
        position: Qt.vector3d(0, 1, 0)
        scale: Qt.vector3d(equipment.assetData.selected ? 0.72 : 0.6, 0.025, equipment.assetData.selected ? 0.72 : 0.6)
        materials: PrincipledMaterial { baseColor: equipment.assetData.selected ? "#ffc96b" : "#344b59"; roughness: 1 }
    }
}
