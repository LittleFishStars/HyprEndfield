import QtQuick
import QtQuick.Shapes

Item {
    id: glyph
    property string name: ""
    property color color: "#E8E8E8"
    width: 24; height: 24
    readonly property string firstPath: {
        switch (name) {
        case "open": return "M3 6h7l2 2h9v11H3z"
        case "folder": return "M3 5h7l2 2h9v13H3z"
        case "apps": return "M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3z"
        case "text": return "M4 5h16M12 5v15M8 20h8"
        case "left": return "M15 4l-8 8 8 8"
        case "right": return "M9 4l8 8-8 8"
        case "bookmark": return "M6 3h12v18l-6-5-6 5z"
        case "gear": return "M8.6 12a3.4 3.4 0 1 0 6.8 0a3.4 3.4 0 1 0-6.8 0zM12 2v4M12 18v4M2 12h4M18 12h4M4.9 4.9l2.8 2.8M16.3 16.3l2.8 2.8M4.9 19.1l2.8-2.8M16.3 7.7l2.8-2.8"
        case "reset": return "M20 12a8 8 0 1 1-2.34-5.66M20 4v8h-8"
        case "x": return "M5.5 5.5l13 13M18.5 5.5l-13 13"
        case "minus": return "M4 12h16"
        case "plus": return "M12 4v16M4 12h16"
        default: return ""
        }
    }
    readonly property string secondPath: {
        if (name === "open") return "M12 11v6M9 14l3-3 3 3"
        if (name === "apps") return "M14 14h7v7h-7z"
        return ""
    }
    readonly property bool firstFilled: name === "apps"
    Shape {
        width: 24; height: 24
        preferredRendererType: Shape.CurveRenderer
        transform: Scale { xScale: glyph.width / 24; yScale: glyph.height / 24 }
        ShapePath {
            strokeColor: glyph.firstFilled ? "transparent" : glyph.color
            strokeWidth: glyph.name === "x" || glyph.name === "text" || glyph.name === "left" || glyph.name === "right"
                || glyph.name === "minus" || glyph.name === "plus" || glyph.name === "reset" ? 3 : 2.6
            capStyle: glyph.name === "x" || glyph.name === "left" || glyph.name === "right" ? ShapePath.SquareCap : ShapePath.FlatCap
            joinStyle: ShapePath.MiterJoin
            fillColor: glyph.firstFilled ? glyph.color : "transparent"
            PathSvg { path: glyph.firstPath }
        }
        ShapePath {
            strokeColor: glyph.color
            strokeWidth: glyph.name === "open" || glyph.name === "apps" ? 2.4 : 2.6
            capStyle: ShapePath.FlatCap; joinStyle: ShapePath.MiterJoin
            fillColor: "transparent"
            PathSvg { path: glyph.secondPath }
        }
    }
}
