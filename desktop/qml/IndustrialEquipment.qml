pragma ComponentBehavior: Bound
import QtQuick
import QtQuick3D

// Finite native geometry, with the caller's original selection identity. Each
// asset instantiates only its own assembly. No cosmetic motion or derived flow.
Node {
    id: visual
    required property string kind
    required property string pickName
    property bool selected: false
    property bool abnormal: false
    property bool showMarker: true
    property real fillFraction: NaN
    readonly property real displayFill: isFinite(fillFraction) ? Math.max(0, Math.min(1, fillFraction)) : 0

    PrincipledMaterial { id: shell; baseColor: visual.abnormal ? "#eb8585" : "#a6b0b8"; metalness: 0.38; roughness: 0.56 }
    PrincipledMaterial { id: bright; baseColor: "#d4dade"; metalness: 0.48; roughness: 0.42 }
    PrincipledMaterial { id: dark; baseColor: "#3c474f"; metalness: 0.28; roughness: 0.64 }
    PrincipledMaterial { id: foundation; baseColor: "#293137"; metalness: 0.05; roughness: 0.94 }
    PrincipledMaterial { id: marking; baseColor: "#a7997d"; metalness: 0.28; roughness: 0.62 }
    PrincipledMaterial { id: window; baseColor: "#17242c"; metalness: 0.18; roughness: 0.3 }
    PrincipledMaterial { id: water; baseColor: visual.abnormal ? "#eb8585" : "#789ba7"; metalness: 0.12; roughness: 0.42 }
    PrincipledMaterial { id: selection; baseColor: visual.selected ? "#edf1f3" : "#354149"; roughness: 0.8 }

    component Part: Model {
        objectName: visual.pickName
        pickable: false
        materials: shell
    }
    Model {
        visible: visual.showMarker
        source: "#Cylinder"; y: 0.8
        scale: Qt.vector3d(visual.selected ? 0.76 : 0.66, 0.018, visual.selected ? 0.76 : 0.66)
        materials: selection
    }
    Loader3D {
        sourceComponent: visual.kind === "tank" ? tankAssembly
            : visual.kind === "pump" ? pumpAssembly
            : visual.kind === "source" || visual.kind === "well" || visual.kind === "disposal" ? wellAssembly
            : visual.kind === "treatment" ? treatmentAssembly
            : visual.kind === "trucking" || visual.kind === "truck_yard" ? truckAssembly
            : visual.kind === "pipeline" ? pipelineAssembly : facilityAssembly
    }

    Component {
        id: tankAssembly
        Node {
            Part { source: "#Cylinder"; y: 3; scale: Qt.vector3d(0.65, 0.06, 0.65); materials: foundation }
            Part { source: "#Cylinder"; y: 40; scale: Qt.vector3d(0.51, 0.68, 0.51); pickable: true }
            Part { source: "#Cylinder"; y: 7; scale: Qt.vector3d(0.55, 0.03, 0.55); materials: dark }
            Part { source: "#Cone"; y: 76; scale: Qt.vector3d(0.53, 0.06, 0.53); materials: bright; pickable: true }
            Part { source: "#Cylinder"; position: Qt.vector3d(-8, 81, 0); scale: Qt.vector3d(0.12, 0.035, 0.12); materials: dark }
            Repeater3D {
                model: [25, 49, 72]
                delegate: Part { required property var modelData; source: "#Cylinder"; y: modelData; scale: Qt.vector3d(0.524, 0.012, 0.524); materials: bright }
            }
            // A bounded sight gauge shows the existing fill. Negative/overflow
            // remains raw in labels/charts and is indicated by abnormal colour.
            Part { source: "#Cube"; position: Qt.vector3d(0, 40, 26.3); scale: Qt.vector3d(0.075, 0.59, 0.018); materials: dark }
            Part { source: "#Cube"; visible: visual.displayFill > 0; position: Qt.vector3d(0, 11 + 28 * visual.displayFill, 27.4); scale: Qt.vector3d(0.042, 0.56 * visual.displayFill, 0.012); materials: water }
            Repeater3D {
                model: [-1, 1]
                delegate: Part { required property var modelData; source: "#Cube"; position: Qt.vector3d(28, 36, modelData * 4); scale: Qt.vector3d(0.014, 0.65, 0.014); materials: dark }
            }
            Repeater3D {
                model: [12, 26, 40, 54, 68]
                delegate: Part { required property var modelData; source: "#Cube"; position: Qt.vector3d(28, modelData, 0); scale: Qt.vector3d(0.015, 0.012, 0.09); materials: bright }
            }
        }
    }
    Component {
        id: pumpAssembly
        Node {
            Part { source: "#Cube"; y: 3; scale: Qt.vector3d(0.69, 0.06, 0.34); materials: foundation }
            Part { source: "#Cube"; position: Qt.vector3d(9, 8, 0); scale: Qt.vector3d(0.3, 0.06, 0.24); materials: dark }
            Part { source: "#Cylinder"; position: Qt.vector3d(12, 17, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.2, 0.3, 0.2); materials: dark; pickable: true }
            Repeater3D {
                model: [3, 12, 21]
                delegate: Part { required property var modelData; source: "#Cylinder"; position: Qt.vector3d(modelData, 17, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.215, 0.018, 0.215); materials: bright }
            }
            Part { source: "#Cylinder"; position: Qt.vector3d(28, 17, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.215, 0.026, 0.215); materials: dark }
            Part { source: "#Cube"; position: Qt.vector3d(12, 29, 0); scale: Qt.vector3d(0.12, 0.06, 0.13); materials: bright }
            Part { source: "#Cube"; position: Qt.vector3d(-8, 14, 0); scale: Qt.vector3d(0.12, 0.12, 0.19); materials: marking }
            Part { source: "#Sphere"; position: Qt.vector3d(-21, 15, 0); scale: Qt.vector3d(0.25, 0.25, 0.19); pickable: true }
            Part { source: "#Cylinder"; position: Qt.vector3d(-35, 15, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.105, 0.14, 0.105); pickable: true }
            Part { source: "#Cylinder"; position: Qt.vector3d(-41, 15, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.17, 0.025, 0.17); materials: bright; pickable: true }
            Part { source: "#Cylinder"; position: Qt.vector3d(-21, 29, 0); scale: Qt.vector3d(0.09, 0.16, 0.09); pickable: true }
            Part { source: "#Cylinder"; position: Qt.vector3d(-21, 37, 0); scale: Qt.vector3d(0.15, 0.025, 0.15); materials: bright }
        }
    }
    Component {
        id: wellAssembly
        Node {
            Part { source: "#Cylinder"; y: 4; scale: Qt.vector3d(visual.kind === "disposal" ? 0.5 : 0.38, 0.08, visual.kind === "disposal" ? 0.5 : 0.38); materials: foundation }
            Part { source: "#Cylinder"; y: 34; scale: Qt.vector3d(0.095, 0.6, 0.095); pickable: true }
            Part { source: "#Cylinder"; y: 13; scale: Qt.vector3d(0.2, 0.035, 0.2); materials: bright }
            Part { source: "#Cylinder"; y: 31; eulerRotation.z: 90; scale: Qt.vector3d(0.085, 0.38, 0.085); pickable: true }
            Part { source: "#Cube"; y: 31; scale: Qt.vector3d(0.15, 0.16, 0.15); materials: dark }
            Part { source: "#Cylinder"; position: Qt.vector3d(22, 31, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.16, 0.025, 0.16); materials: marking }
            Part { source: "#Cylinder"; position: Qt.vector3d(23.5, 31, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.11, 0.028, 0.11); materials: dark }
            Part { source: "#Cylinder"; y: 52; eulerRotation.x: 90; scale: Qt.vector3d(0.075, 0.3, 0.075); pickable: true }
            Part { source: "#Cylinder"; position: Qt.vector3d(0, 52, 16); eulerRotation.x: 90; scale: Qt.vector3d(0.145, 0.025, 0.145); materials: bright }
            Part { source: "#Cylinder"; y: 66; scale: Qt.vector3d(0.16, 0.035, 0.16); materials: bright }
            Part { source: "#Cube"; position: Qt.vector3d(11, 61, 0); scale: Qt.vector3d(0.13, 0.12, 0.09); materials: visual.kind === "disposal" ? water : dark }
        }
    }
    Component {
        id: treatmentAssembly
        Node {
            Part { source: "#Cube"; y: 3; scale: Qt.vector3d(0.95, 0.06, 0.57); materials: foundation }
            Repeater3D {
                model: [-1, 1]
                delegate: Node {
                    required property var modelData
                    x: modelData * 23
                    Part { source: "#Cylinder"; y: 29; scale: Qt.vector3d(0.25, 0.44, 0.25); pickable: true }
                    Part { source: "#Sphere"; y: 51; scale: Qt.vector3d(0.25, 0.09, 0.25); materials: bright }
                    Part { source: "#Cylinder"; y: 11; scale: Qt.vector3d(0.28, 0.025, 0.28); materials: dark }
                }
            }
            Part { source: "#Cylinder"; y: 40; eulerRotation.z: 90; scale: Qt.vector3d(0.065, 0.48, 0.065); materials: bright }
            Part { source: "#Cube"; position: Qt.vector3d(0, 17, 24); scale: Qt.vector3d(0.2, 0.24, 0.1); materials: dark; pickable: true }
            Part { source: "#Cube"; position: Qt.vector3d(0, 20, 29.5); scale: Qt.vector3d(0.13, 0.08, 0.012); materials: window }
        }
    }
    Component {
        id: truckAssembly
        Node {
            scale: visual.kind === "truck_yard" ? Qt.vector3d(0.75, 0.75, 0.75) : Qt.vector3d(1, 1, 1)
            Part { source: "#Cube"; y: 12; scale: Qt.vector3d(0.97, 0.06, 0.34); materials: dark }
            Part { source: "#Cylinder"; position: Qt.vector3d(-12, 31, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.29, 0.62, 0.29); pickable: true }
            Part { source: "#Sphere"; position: Qt.vector3d(-44, 31, 0); scale: Qt.vector3d(0.09, 0.29, 0.29); materials: bright }
            Part { source: "#Cube"; position: Qt.vector3d(34, 26, 0); scale: Qt.vector3d(0.27, 0.29, 0.32); pickable: true }
            Part { source: "#Cube"; position: Qt.vector3d(35, 31, 16.5); scale: Qt.vector3d(0.2, 0.12, 0.012); materials: window }
            Repeater3D {
                model: [-31, 30]
                delegate: Node {
                    required property var modelData
                    x: modelData
                    Part { source: "#Cylinder"; position: Qt.vector3d(0, 9, -19); eulerRotation.x: 90; scale: Qt.vector3d(0.16, 0.065, 0.16); materials: foundation }
                    Part { source: "#Cylinder"; position: Qt.vector3d(0, 9, 19); eulerRotation.x: 90; scale: Qt.vector3d(0.16, 0.065, 0.16); materials: foundation }
                }
            }
            Repeater3D {
                model: [-28, 8]
                delegate: Part { required property var modelData; source: "#Cylinder"; position: Qt.vector3d(modelData, 31, 0); eulerRotation.z: 90; scale: Qt.vector3d(0.302, 0.015, 0.302); materials: dark }
            }
        }
    }
    Component {
        id: pipelineAssembly
        Node {
            Part { source: "#Cube"; y: 3; scale: Qt.vector3d(0.84, 0.06, 0.31); materials: foundation }
            IndustrialPipe { pickName: visual.pickName; span: 72; diameter: 10; y: 15; eulerRotation.z: 90 }
            Part { source: "#Cylinder"; y: 25; scale: Qt.vector3d(0.035, 0.2, 0.035); materials: dark }
            Part { source: "#Cylinder"; y: 35; scale: Qt.vector3d(0.2, 0.02, 0.2); materials: bright }
        }
    }
    Component {
        id: facilityAssembly
        Node {
            // Overview assemblies stay deliberately lighter than drill-downs.
            Part { source: "#Cube"; y: 2; scale: Qt.vector3d(0.78, 0.04, 0.57); materials: foundation }
            Repeater3D {
                model: [-1, 1]
                delegate: Node {
                    required property var modelData
                    x: modelData * 17
                    Part { source: "#Cylinder"; y: 25; scale: Qt.vector3d(0.27, 0.45, 0.27); pickable: true }
                    Part { source: "#Cone"; y: 49; scale: Qt.vector3d(0.28, 0.05, 0.28); materials: bright }
                    Part { source: "#Cylinder"; y: 12; scale: Qt.vector3d(0.28, 0.015, 0.28); materials: dark }
                }
            }
            Part { source: "#Cube"; position: Qt.vector3d(0, 13, 26); scale: Qt.vector3d(0.3, 0.22, 0.17); materials: dark; pickable: true }
        }
    }
}
