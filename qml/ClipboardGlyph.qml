import QtQuick
import QtQuick.Shapes

// EndfieldHUD HUDIcon paths, authored on a 24 × 24 grid.
Item {
    id: glyph
    property string kind: "text"
    property color ink: "#F2F2F2"
    function strokePath() {
        switch (kind) {
        case "copy": return "M8 8h12v12H8zM4 16V4h12";
        case "text": return "M4 5h16M12 5v15M8 20h8";
        case "link": return "M10 14l4-4M8.5 11.5l-2 2a3.5 3.5 0 0 0 5 5l2-2M15.5 12.5l2-2a3.5 3.5 0 0 0-5-5l-2 2";
        case "image": return "M3 4h18v16H3z";
        case "file": return "M5 3h9l5 5v13H5zM14 3v5h5";
        case "folder": return "M3 5h7l2 2h9v13H3z";
        case "x": return "M5.5 5.5l13 13M18.5 5.5l-13 13";
        case "trash": return "M4 6h16M9 6V3h6v3M6 6l1 15h10l1-15";
        case "pin": return "M7 3h10M9.5 3v6.5L6 14h12l-3.5-4.5V3M12 14v7";
        default: return "";
        }
    }
    function fillPath() {
        return kind === "image" ? "M5 18l5-6 4 4 2-2 3 4zM14 7h3v3h-3z" : "";
    }
    Item {
        width: 24; height: 24
        transform: Scale { xScale: glyph.width / 24; yScale: glyph.height / 24 }
        Shape {
            anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"; strokeColor: glyph.ink
                strokeWidth: glyph.kind === "pin" ? 2.4 : 2.6
                capStyle: ShapePath.FlatCap; joinStyle: ShapePath.MiterJoin
                PathSvg { path: glyph.strokePath() }
            }
            ShapePath {
                strokeWidth: -1; fillColor: glyph.ink
                PathSvg { path: glyph.fillPath() }
            }
        }
    }
}
