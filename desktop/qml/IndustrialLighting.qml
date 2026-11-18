import QtQuick
import QtQuick3D

Node {
    id: lighting
    property int quality: 1
    DirectionalLight {
        eulerRotation: Qt.vector3d(-52, -28, 0)
        color: "#FFFAEF"; brightness: 1.25; ambientColor: "#A8B6BD"
        castsShadow: lighting.quality > 0; shadowFactor: 38; shadowBias: 3
    }
    DirectionalLight { eulerRotation: Qt.vector3d(-30, 135, 0); color: "#DBE9F2"; brightness: 0.35 }
}
