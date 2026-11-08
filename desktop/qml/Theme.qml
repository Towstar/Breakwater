pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#0D0F11"
    readonly property color surface: "#15181B"
    readonly property color raised: "#1D2125"
    readonly property color hover: "#262B30"
    readonly property color selected: "#2D3339"
    readonly property color divider: "#333A42"
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
    readonly property int cornerRadius: 4
    function icon(name) { return name.length ? "qrc:/breakwater/assets/icons/" + name + ".svg" : "" }
}
