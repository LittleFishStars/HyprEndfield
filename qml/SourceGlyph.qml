import QtQuick
import QtQuick.Shapes

// EndfieldHUD Sources/HUDDesignIcons.swift, original 24-unit paths.
Item {
    id:glyph; property string kind:""; property color ink:"#EEEEEE"
    function circle(x,y,r) {return "M"+(x-r)+" "+y+"a"+r+" "+r+" 0 1 0 "+(2*r)+" 0a"+r+" "+r+" 0 1 0 "+(-2*r)+" 0z"}
    function line() {
        switch(kind) {
        case "authority":return "M5 10L15 17V30L5 23zM10 6L22 14V28M17 2L29 10V25"
        case "rotate":return "M20 12a8 8 0 1 1-3-6.2"
        case "mirror":return "M12 3v18"
        case "gear":return circle(12,12,3.4)+"M12 2v4M12 18v4M2 12h4M18 12h4M4.9 4.9l2.8 2.8M16.3 16.3l2.8 2.8M4.9 19.1l2.8-2.8M16.3 7.7l2.8-2.8"
        case "grid":return "M3 3h18v18H3zM3 9h18M3 15h18M9 3v18M15 3v18"
        case "lyric":return "M4 6h12M4 11h12M4 16h7M19 9v9"
        case "apps":return "M14 14h7v7h-7z"
        case "pen":return "M4 20l1-5L16 4l4 4L9 19z"
        case "eraser":return "M3 17l9-11 8 7-7 8H6zM8 11l7 6"
        case "image":return "M3 4h18v16h-18z"
        case "eye":case "eyeoff":return "M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z"+(kind === "eyeoff" ? "M4 21L20 3" : "")
        case "timer":return circle(12,13,8)+"M12 9v4l3 2M9 2h6"
        case "reset":return "M5 12a7 7 0 1 0 2-4.9"
        case "back":return "M15 4l-8 8 8 8"
        case "trash":return "M4 6h16M9 6V3h6v3M6 6l1 15h10l1-15"
        case "export":return "M12 15V3M7 8l5-5 5 5M4 14v7h16v-7"
        case "ext":return "M14 4h6v6M20 4l-9 9M18 14v6H4V6h6"
        default:return ""
        }
    }
    function fill() {
        switch(kind) {
        case "exploration":return "M8 8L12 4V10zM22 8L25 4L26 12zM8 24L12 28V22zM22 24L25 28L26 20zM5 5L7 10L6 16L7 22L5 27L3 21V11zM1 8L2 13V19L1 24L0 20V12zM29 5L27 10L28 16L27 22L29 27L31 21V11zM33 8L32 13V19L33 24L34 20V12z"
        case "play":return "M7 4l13 8-13 8z"
        case "pause":return "M6 4h4v16H6zM14 4h4v16h-4z"
        case "rotate":return "M20 3v6h-6z"
        case "mirror":return "M10 6L3 18h7zM14 6l7 12h-7z"
        case "lyric":return circle(17,18,2.6)
        case "apps":return "M3 3h7v7h-7zM14 3h7v7h-7zM3 14h7v7h-7z"
        case "image":return "M5 18l5-6 4 4 2-2 3 4zM14 7h3v3h-3z"
        case "eye":case "eyeoff":return circle(12,12,3)
        case "reset":return "M5 3v5h5z"
        default:return ""
        }
    }
    Item { width:24;height:24;transform:Scale { xScale:glyph.width/(glyph.kind === "authority" || glyph.kind === "exploration" ? 34 : 24);yScale:glyph.height/(glyph.kind === "authority" || glyph.kind === "exploration" ? 32 : 24) }
        Shape { anchors.fill:parent;preferredRendererType:Shape.CurveRenderer
            ShapePath { fillColor:"transparent";strokeColor:glyph.ink;strokeWidth:glyph.kind === "authority" ? 2.7 : glyph.kind === "mirror" || glyph.kind === "grid" ? 2 : glyph.kind === "reset" ? 2.8 : glyph.kind === "back" ? 3.2 : glyph.kind === "apps" || glyph.kind === "eye" ? 2.4 : 2.6;capStyle:glyph.kind === "back" || glyph.kind === "authority" ? ShapePath.SquareCap : ShapePath.RoundCap;joinStyle:glyph.kind === "authority" ? ShapePath.MiterJoin : ShapePath.RoundJoin;strokeStyle:glyph.kind === "mirror" ? ShapePath.DashLine : ShapePath.SolidLine;dashPattern:[1,1]; PathSvg {path:glyph.line()} }
            ShapePath { fillColor:glyph.ink;strokeWidth:-1;fillRule:ShapePath.OddEvenFill;PathSvg {path:glyph.kind === "exploration" ? glyph.circle(17,16,10)+glyph.circle(17,16,4) : ""} }
            ShapePath { fillColor:glyph.ink;strokeWidth:-1;PathSvg {path:glyph.fill()} }
        }
    }
}
