import QtQuick
import QtQuick3D

Node {
    id: lighting
    property int quality: 1
    DirectionalLight {
        eulerRotation: Qt.vector3d(-52, -28, 0)
        color: "#F6FAFC"; brightness: 1.15; ambientColor: "#B2C0C6"
        castsShadow: lighting.quality > 0; shadowFactor: 38; shadowBias: 3
    }
    DirectionalLight { eulerRotation: Qt.vector3d(-30, 135, 0); color: "#E2EAF0"; brightness: 0.4 }
}
