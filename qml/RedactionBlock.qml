import QtQuick

Item {
    id: block
    property string content: ""
    readonly property var lines: {
        let out = [];
        for (let paragraph of content.split(/\r?\n/)) {
            let line = "";
            for (let word of paragraph.split(" ")) {
                while ([...word].length > 46) { if (line) { out.push(line); line = ""; } out.push([...word].slice(0, 46).join("")); word = [...word].slice(46).join(""); }
                if (!line) line = word;
                else if ([...line].length + 1 + [...word].length <= 46) line += " " + word;
                else { out.push(line); line = word; }
            }
            out.push(line);
        }
        return out.slice(0, Math.max(0, Math.floor((height - 32 + 9) / 22)));
    }
    Repeater {
        model: block.lines
        Redaction { required property string modelData; required property int index; x: 14; y: 16 + index * 22; width: Math.min(block.width - 28, Math.max(60, [...modelData].length * 9.2)); labelled: index === 0 }
    }
}
