import QtQuick3D

SceneEnvironment {
    property int quality: 1
    backgroundMode: SceneEnvironment.Color
    clearColor: "#9DAFB8"
    antialiasingMode: quality === 0 ? SceneEnvironment.NoAA : SceneEnvironment.MSAA
    antialiasingQuality: quality === 2 ? SceneEnvironment.High : SceneEnvironment.Medium
    aoEnabled: quality === 2
    aoStrength: 18; aoDistance: 12; aoSoftness: 35
    tonemapMode: SceneEnvironment.TonemapModeFilmic
}
