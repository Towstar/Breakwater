pragma Singleton
import QtQuick

QtObject {
    property bool compactControls: false
    readonly property int controlHeight: compactControls ? 30 : 36
    readonly property color background: "#101316"
    readonly property color surface: "#181D22"
    readonly property color raised: "#21282F"
    readonly property color hover: "#2B353E"
    readonly property color selected: "#30434F"
    readonly property color divider: "#303A43"
    readonly property color accent: "#92BDD0"
    readonly property color overlay: "#EB181D22"
    readonly property color overlayBorder: "#52606C"
    readonly property color controlBorder: "#68727D"
    readonly property color text: "#F3F3F3"
    readonly property color primaryHover: "#FFFFFF"
    readonly property color primaryPressed: "#D5D9DD"
    readonly property color secondary: "#BBC2C9"
    readonly property color muted: "#929CA6"
    readonly property color warning: "#E4B55B"
    readonly property color error: "#EB8585"
    readonly property color success: "#C3D5C8"
    readonly property color plotReference: "#A6AFB8"
    readonly property color plotCandidate: "#F3F3F3"
    readonly property string fontFamily: "Segoe UI"
    readonly property string monoFamily: "Consolas"
    readonly property int bodySize: 13
    readonly property int smallSize: 12
    readonly property int captionSize: 11
    readonly property int cornerRadius: 6
    function icon(name) { return name.length ? "qrc:/breakwater/assets/icons/" + name + ".svg" : "" }
    function equipmentIcon(kind) {
        return icon(({tank:"cylinder",source:"waves",treatment:"factory",trucking:"truck",pump:"gauge",route:"route"})[kind] || "boxes")
    }
    function equipmentName(kind) {
        return ({tank:"Storage tank",source:"Production source",treatment:"Treatment",trucking:"Tanker outlet",pump:"Pump",route:"Pipe route"})[kind] || "Equipment"
    }
}
