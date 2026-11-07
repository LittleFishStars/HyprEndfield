import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

Item {
    id: glyph
    property string kind: "text"
    property color ink: Appearance.tone("#F2F2F2")

    function path() {
        switch (kind) {
        case "text": return "M4 5h16M12 5v15M8 20h8"
        case "todo": return "M4 6l2 2 3-4M11 6h9M4 13l2 2 3-4M11 13h9M4 20l2 2 3-4M11 20h9"
        case "image": return "M3 4h18v16H3zM5 18l5-6 4 4 2-2 3 4M14 7h3v3h-3z"
        case "pen": return "M4 20l4.5-1 10-10-3.5-3.5-10 10zM13.5 7l3.5 3.5"
        case "pin": return "M7 3h10M9.5 3v6.5L6 14h12l-3.5-4.5V3M12 14v7"
        case "trash": return "M4 6h16M9 6V3h6v3M6 6l1 15h10l1-15"
        case "close": return "M5 5l14 14M19 5L5 19"
        case "check": return "M4 13l5 5L20 6"
        case "up": return "M5 15l7-7 7 7"
        case "down": return "M5 9l7 7 7-7"
        case "flag": return "M6 3v18M7 4h11l-3 4 3 4H7"
        case "eraser": return "M5 17l8-10 6 5-7 8H8zM12 20h8"
        default: return ""
        }
    }

    Item {
        width: 24; height: 24
        transform: Scale { xScale: glyph.width / 24; yScale: glyph.height / 24 }
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"
                strokeColor: glyph.ink
                strokeWidth: 2.2
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: glyph.path() }
            }
        }
    }
}
