import QtQuick
import QtQuick.Shapes
import QtQuick.Dialogs
import QtQuick.Controls as Controls

// EndfieldHUD ReaderCanvas (MOD-10): Open / Library / Reading / Marks.
FocusScope {
    id: readerPage
    width: 590; height: 510; focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property var readerState: readerController.state
    readonly property var books: readerState.books || []
    readonly property var currentBook: readerState.book || ({})
    readonly property var currentPage: readerState.current || ({})
    readonly property var preferences: readerState.preferences || ({ vertical: true, fontName: "Georgia", fontSize: 10, lineSpacing: 2, margin: 16 })
    property int tab: 1
    property int pendingTab: -1
    property int pendingDirection: 0
    property int pageTurnDirection: 1
    property string menuBookId: ""
    property point menuPoint: Qt.point(0, 0)
    property string relinkBookId: ""
    property bool shelfMenuOpen: false
    property bool settingsOpen: false
    property bool fontMenuOpen: false
    property var fonts: []
    property string confirmBookId: ""
    property bool toastOpen: false
    property string toastTitle: ""
    property string toastMessage: ""
    property string lastError: ""

    function tabKey(index) { return ["open", "library", "reading", "marks"][Math.max(0, Math.min(3, index))] }
    function notify(title, message) { toastTitle = title; toastMessage = message || ""; toastOpen = true; toastTimer.restart() }
    function coverColor(id) {
        const colors = ["#2A5D7A", "#7A2A2A", "#555555", "#6B5A12", "#3D5A2A", "#4A3A6B", "#2A4A4A", "#6B3A1E"]
        const hex = String(id || "").split("-").join(""); let sum = 0
        for (let i = 0; i + 1 < hex.length; i += 2) sum = (sum + parseInt(hex.slice(i, i + 2), 16)) & 255
        return colors[sum % colors.length]
    }
    function shownBooks() { return books.map(value => Object.assign({}, value, { cover: coverColor(value.id) })) }
    function supportedUrl(url) { return /\.(pdf|epub|txt)$/i.test(String(url).split(/[?#]/)[0]) }
    function selectTab(index, direction) {
        if (index === tab || tabAnimation.running) return
        closePopups(); pendingTab = index; pendingDirection = direction || (index > tab ? 1 : -1)
        if (reduceMotion) { performTab(); return }
        tabAnimation.restart()
    }
    function performTab() {
        tab = pendingTab; readerController.setTab(tabKey(tab)); pendingTab = -1
        bodyHost.x = pendingDirection * 24; bodyHost.opacity = 0
        if (reduceMotion) { bodyHost.x = 0; bodyHost.opacity = 1 }
    }
    function cycleTab(step) {
        const next = (tab + step + 4) % 4
        selectTab(next, step > 0 ? 1 : -1)
    }
    function closePopups() { menuBookId = ""; shelfMenuOpen = false; settingsOpen = false; fontMenuOpen = false }
    function openBook(entry) {
        closePopups()
        if (!entry.available) { relinkBookId = entry.id; bookDialog.open(); return }
        readerController.selectBook(entry.id); selectTab(2, 1)
    }
    function turn(direction) {
        if (!currentPage.key) return
        pageTurnDirection = direction
        if (paper.verticalMode) {
            paper.scrollPage(direction)
            return
        }
        readerController.turn(direction)
    }

    component IconButton: Item {
        id: iconButton
        property string icon: ""
        property bool checked: false
        property bool reduceMotion: false
        signal clicked()
        width: 30; height: 30; opacity: enabled ? 1 : 0.3
        Rectangle { anchors.fill: parent; color: pointer.containsMouse && iconButton.enabled ? "#303030" : iconButton.checked ? "#FFFA00" : "#202020"; border.color: pointer.containsMouse ? "#8A8A8A" : "#555555" }
        ReaderGlyph { anchors.centerIn: parent; width: 15; height: 15; name: iconButton.icon; color: iconButton.checked ? "#171717" : "#E8E8E8" }
        MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; enabled: iconButton.enabled; cursorShape: Qt.PointingHandCursor; onClicked: iconButton.clicked() }
    }
    component EmptyState: Item {
        property string title: "EMPTY"
        property string subtitle: ""
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#4B4B4B" }
        Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: -1; fillColor: "#0AFFFFFF"; PathSvg { path: { let p = ""; for (let x = -300; x < parent.parent.width; x += 7) p += "M" + x + " " + parent.parent.height + "h1.414l" + parent.parent.height + " -" + parent.parent.height + "h-1.414Z"; return p } } } }
        Row { anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height - 42) / 2; spacing: 4
            Text { text: "[ "; color: "#FFFA00"; font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
            Text { text: parent.parent.title; color: "#E9E9E9"; font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
            Text { text: " ]"; color: "#FFFA00"; font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height - 42) / 2 + 30; text: parent.subtitle; visible: text.length > 0; color: "#8A8A8A"; font.family: harmonyRegular.name; font.pixelSize: 9; font.letterSpacing: 1.38; renderType: Text.CurveRendering }
    }

    onPresentedChanged: {
        readerController.active = presented
        if (presented) { forceActiveFocus(); readerController.refresh() }
        else closePopups()
    }
    Component.onCompleted: {
        tab = readerState.reading && currentBook.id ? 2 : 1
        readerController.setTab(tabKey(tab))
    }
    Component.onDestruction: readerController.active = false

    Connections {
        target: readerController
        function onStateChanged() {
            const next = readerController.state
            if (next.error && next.error !== readerPage.lastError) { readerPage.lastError = next.error; readerPage.notify("阅读器", next.error) }
            paper.syncPage()
        }
        function onActionFinished(action, success, message) {
            if (success && action === "import") readerPage.selectTab(2, 1)
            readerPage.notify(success ? "阅读器" : "提示", message)
        }
    }

    FileDialog {
        id: bookDialog
        title: readerPage.relinkBookId ? "重新链接书籍" : "选择书籍"
        fileMode: FileDialog.OpenFile
        nameFilters: ["电子书 (*.pdf *.epub *.txt)"]
        options: FileDialog.DontUseNativeDialog | FileDialog.DontResolveSymlinks
        Component.onCompleted: { if ("popupType" in bookDialog) bookDialog.popupType = Controls.Popup.Item }
        onAccepted: {
            if (readerPage.relinkBookId) readerController.relinkBook(readerPage.relinkBookId, selectedFile)
            else readerController.importUrl(selectedFile)
            readerPage.relinkBookId = ""; readerPage.forceActiveFocus()
        }
        onRejected: { readerPage.relinkBookId = ""; readerPage.forceActiveFocus() }
    }

    SequentialAnimation {
        id: tabAnimation
        ParallelAnimation {
            NumberAnimation { target: bodyHost; property: "opacity"; to: 0; duration: 130; easing.type: Easing.InOutQuad }
            NumberAnimation { target: bodyHost; property: "x"; to: -0.6 * readerPage.pendingDirection * 24; duration: 130; easing.type: Easing.InOutQuad }
        }
        ScriptAction { script: readerPage.performTab() }
        ParallelAnimation {
            NumberAnimation { target: bodyHost; property: "opacity"; to: 1; duration: 476; easing.type: Easing.OutCubic }
            NumberAnimation { target: bodyHost; property: "x"; to: 0; duration: 476; easing.type: Easing.OutCubic }
        }
    }

    Item {
        id: header; width: 590; height: 81
        Row {
            x: 0; y: 1.75; spacing: 6
            Repeater {
                model: readerPage.tab === 2 && readerPage.currentPage.key && readerPage.currentBook.title
                    ? ["// HUD", "/", "E-READER", "/", String(readerPage.currentBook.title).length > 44 ? String(readerPage.currentBook.title).slice(0, 43) + "…" : readerPage.currentBook.title]
                    : ["// HUD", "/", "E-READER"]
                Text { required property string modelData; required property int index; text: modelData; color: index >= 2 ? "#E6E6E6" : "#8A8A8A"; font.family: harmonyRegular.name; font.pixelSize: 8; font.letterSpacing: 0.63; renderType: Text.CurveRendering }
            }
        }
        Text { x: 549 - width; y: 1.75; text: "MOD-10"; color: "#8A8A8A"; font.family: harmonyRegular.name; font.pixelSize: 8; font.letterSpacing: 0.63; renderType: Text.CurveRendering }
        Item { x: 556; y: 3.5; width: 34; height: 7; clip: true; Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: -1; fillColor: "#24FFFFFF"; PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p } } } } }
        Text { id: heading; x: 0; y: 20; height: 30; text: "阅读器"; color: "#F4F4F4"; font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08; renderType: Text.CurveRendering }
        Text { x: heading.width + 12; y: heading.y + heading.baselineOffset - baselineOffset; text: "E-READER"; color: "#8A8A8A"; font.family: harmonyMedium.name; font.pixelSize: 13; font.weight: Font.Medium; font.letterSpacing: 2.6; renderType: Text.CurveRendering }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: "#FFFA00" }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - counter.width - 8; y: -1; width: counter.width + 8; height: 12; color: "#151515" }
            Text { id: counter; x: 590 - width; y: -1.5; text: readerPage.tab === 2 && readerPage.currentPage.number ? "P. " + readerPage.currentPage.number : String(readerPage.books.length).padStart(2, "0"); color: "#FFFA00"; font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering }
        }
    }

    ReaderTabs { id: tabs; objectName: "readerTabs"; x: 0; y: 81; currentIndex: readerPage.tab; reduceMotion: readerPage.reduceMotion; onSelected: (index, direction) => readerPage.selectTab(index, direction) }

    Item {
        id: bodyHost; anchors.fill: parent

        Item {
            id: openPage; visible: readerPage.tab === 0; x: 0; y: 125; width: 590; height: 300
            EmptyState { anchors.fill: parent; title: "OPEN A BOOK"; subtitle: "PDF / EPUB / TXT" }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter; y: 190; spacing: 8
                CalendarButton { width: 104; height: 26; title: "选择文件"; reduceMotion: readerPage.reduceMotion; onClicked: { readerPage.relinkBookId = ""; bookDialog.open() } }
                CalendarButton { width: 112; height: 26; title: "从暂存架"; kind: "secondary"; enabled: fileShelfController.readerChoices().length > 0; reduceMotion: readerPage.reduceMotion; onClicked: { readerPage.shelfMenuOpen = !readerPage.shelfMenuOpen } }
            }
        }

        Item {
            id: libraryPage; objectName: "readerLibrary"; visible: readerPage.tab === 1; x: 0; y: 125; width: 590; height: 385
            GridView {
                id: libraryGrid; anchors.fill: parent; clip: true; boundsBehavior: Flickable.StopAtBounds
                cellWidth: 147.5; cellHeight: 156; model: readerPage.shownBooks(); cacheBuffer: 312
                delegate: ReaderBookTile {
                    required property var modelData; required property int index
                    x: 4; width: 135.5; entry: modelData; selected: modelData.id === readerPage.readerState.selected; reduceMotion: readerPage.reduceMotion
                    onOpened: readerPage.openBook(modelData)
                    onMoreRequested: (localX, localY) => {
                        const point = mapToItem(readerPage, localX, localY)
                        readerPage.menuBookId = modelData.id; readerPage.menuPoint = Qt.point(Math.min(420, point.x), Math.min(408, point.y))
                    }
                }
                Controls.ScrollBar.vertical: Controls.ScrollBar { policy: libraryGrid.contentHeight > libraryGrid.height ? Controls.ScrollBar.AlwaysOn : Controls.ScrollBar.AlwaysOff; width: 4 }
            }
            EmptyState { anchors.fill: parent; visible: readerPage.books.length === 0; title: "NO BOOKS"; subtitle: "PDF / EPUB / TXT" }
            DropArea {
                anchors.fill: parent; enabled: readerPage.presented && !readerPage.readerState.busy
                onEntered: drag => { drag.accepted = drag.hasUrls && drag.urls.some(url => readerPage.supportedUrl(url)) }
                onDropped: drop => {
                    const found = drop.urls.find(url => readerPage.supportedUrl(url))
                    if (found && readerController.importUrl(found)) { drop.action = Qt.CopyAction; drop.accepted = true }
                }
            }
        }

        Item {
            id: readingPage; visible: readerPage.tab === 2; anchors.fill: parent
            EmptyState {
                x: 0; y: 121; width: 590; height: 351
                visible: !readerPage.currentPage.key
                title: readerPage.readerState.loading ? "LOADING" : readerPage.readerState.error ? "UNAVAILABLE" : "OPEN A BOOK"
                subtitle: readerPage.readerState.loading ? "" : "PDF / EPUB / TXT"
            }
            Rectangle {
                id: paper; objectName: "readerPaper"; x: 0; y: 121; width: 590; height: 351
                visible: !!readerPage.currentPage.key; color: "#EFEDE6"; border.color: "#C9C7BF"; clip: true
                readonly property bool verticalMode: readerPage.preferences.vertical && readerPage.readerState.zoom <= 1.0001
                property string shownKey: ""
                property string shownSource: ""
                property string shownBaseSource: ""
                property bool shownDetail: false
                property bool shownIllustration: false
                property string outgoingSource: ""
                property real outgoingX: 0
                property real outgoingY: 0
                property real outgoingWidth: 588
                property real outgoingHeight: 349
                property int pendingVerticalDirection: 0
                property real verticalCarry: 0
                property bool adjustingColumn: false
                readonly property real columnInset: Math.round(Math.min(349 * 0.4, Number(readerPage.preferences.margin || 16) * 1.4))
                readonly property var columnPages: orderedPages()
                readonly property real currentTop: {
                    let value = 0
                    for (const page of columnPages) { if (page.role === "current") return value; value += Number(page.columnHeight || 1) }
                    return 0
                }
                readonly property real currentHeight: {
                    const value = columnPages.find(page => page.role === "current")
                    return value ? Number(value.columnHeight || 1) : 1
                }
                function orderedPages() {
                    const values = readerPage.readerState.pages || []
                    return values.filter(value => value.role === "previous")
                        .concat(values.filter(value => value.role === "current"))
                        .concat(values.filter(value => value.role === "next"))
                }
                function syncPage() {
                    const page = readerController.state.current || ({})
                    if (!page.key) { shownKey = ""; shownSource = ""; return }
                    const source = page.detailReady ? page.source : page.baseSource
                    if (!shownKey) {
                        shownKey = page.key; shownSource = source; shownBaseSource = page.baseSource; shownDetail = page.detailReady; shownIllustration = page.illustration
                        Qt.callLater(resetColumn); return
                    }
                    if (page.key === shownKey) {
                        shownSource = source; shownBaseSource = page.baseSource; shownDetail = page.detailReady; shownIllustration = page.illustration
                        return
                    }
                    if (verticalMode) {
                        shownKey = page.key; shownSource = source; shownBaseSource = page.baseSource; shownDetail = page.detailReady; shownIllustration = page.illustration
                        Qt.callLater(resetColumn); return
                    }
                    outgoingSource = shownSource; outgoingX = incomingImage.x; outgoingY = incomingImage.y; outgoingWidth = incomingImage.width; outgoingHeight = incomingImage.height
                    shownKey = page.key; shownSource = source; shownBaseSource = page.baseSource; shownDetail = page.detailReady; shownIllustration = page.illustration
                    incomingLayer.x = readerPage.pageTurnDirection * 588; outgoingLayer.x = 0
                    if (readerPage.reduceMotion) { incomingLayer.x = 0; outgoingLayer.x = -readerPage.pageTurnDirection * 588; outgoingSource = "" }
                    else pageSlide.restart()
                }
                function resetColumn() {
                    if (!verticalMode || !readerPage.currentPage.key) return
                    adjustingColumn = true
                    if (pendingVerticalDirection > 0) column.contentY = currentTop + Math.max(0, verticalCarry)
                    else if (pendingVerticalDirection < 0) column.contentY = currentTop + currentHeight + verticalCarry
                    else column.contentY = currentTop
                    pendingVerticalDirection = 0; verticalCarry = 0
                    Qt.callLater(() => adjustingColumn = false)
                }
                function inspectColumn() {
                    if (adjustingColumn || !verticalMode || pendingVerticalDirection !== 0 || !readerPage.currentPage.key) return
                    if (column.contentY >= currentTop + currentHeight - 0.5 && readerPage.currentPage.canNext) {
                        pendingVerticalDirection = 1; verticalCarry = column.contentY - (currentTop + currentHeight); readerController.turn(1)
                    } else if (column.contentY < currentTop - 0.5 && readerPage.currentPage.canPrevious) {
                        pendingVerticalDirection = -1; verticalCarry = column.contentY - currentTop; readerController.turn(-1)
                    }
                }
                function scrollPage(direction) {
                    if (!verticalMode) return
                    let target
                    if (direction > 0) {
                        if (!readerPage.currentPage.canNext) return
                        target = currentTop + currentHeight
                    } else if (column.contentY > currentTop + 0.5) target = currentTop
                    else {
                        const previous = columnPages.find(value => value.role === "previous")
                        if (!previous) { readerController.turn(-1); return }
                        target = currentTop - Number(previous.columnHeight || 1)
                    }
                    verticalScroll.stop(); verticalScroll.from = column.contentY; verticalScroll.to = target
                    if (readerPage.reduceMotion) column.contentY = target; else verticalScroll.start()
                }

                Item {
                    id: horizontalViewport; x: 1; y: 1; width: 588; height: 349; clip: true; visible: !paper.verticalMode
                    Item {
                        id: outgoingLayer; width: 588; height: 349; visible: paper.outgoingSource.length > 0
                        Image { x: paper.outgoingX; y: paper.outgoingY; width: paper.outgoingWidth; height: paper.outgoingHeight; source: readingPage.visible ? paper.outgoingSource : ""; sourceSize: Qt.size(1260, 748); fillMode: Image.Stretch; smooth: true; mipmap: true; asynchronous: false }
                    }
                    Item {
                        id: incomingLayer; width: 588; height: 349
                        Image {
                            id: incomingImage
                            x: paper.shownDetail ? 0 : (1 - readerPage.readerState.zoom) * 294 + readerPage.readerState.panX * 1.4
                            y: paper.shownDetail ? 0 : (1 - readerPage.readerState.zoom) * 174.5 + readerPage.readerState.panY * 1.4
                            width: paper.shownDetail ? 588 : 588 * readerPage.readerState.zoom
                            height: paper.shownDetail ? 349 : 349 * readerPage.readerState.zoom
                            source: readingPage.visible ? paper.shownSource : ""; sourceSize: Qt.size(1260, 748); fillMode: Image.Stretch; smooth: true; mipmap: true; asynchronous: false
                        }
                    }
                    ParallelAnimation {
                        id: pageSlide
                        NumberAnimation { target: incomingLayer; property: "x"; to: 0; duration: 300; easing.type: Easing.OutCubic }
                        NumberAnimation { target: outgoingLayer; property: "x"; to: -readerPage.pageTurnDirection * 588; duration: 300; easing.type: Easing.OutCubic }
                        onFinished: paper.outgoingSource = ""
                    }
                }

                Flickable {
                    id: column; objectName: "readerVerticalColumn"; x: 1; y: 1 + paper.columnInset; width: 588; height: 349 - 2 * paper.columnInset
                    visible: paper.verticalMode; clip: true; contentWidth: width
                    contentHeight: columnContent.height; boundsBehavior: Flickable.StopAtBounds; flickDeceleration: 2600
                    onContentYChanged: paper.inspectColumn()
                    WheelHandler {
                        target: null
                        enabled: paper.verticalMode && !!readerPage.currentPage.key
                        acceptedModifiers: Qt.ControlModifier
                        orientation: Qt.Vertical
                        blocking: true
                        onWheel: event => {
                            const delta = event.angleDelta.y || event.pixelDelta.y
                            if (!delta) return
                            const point = column.mapToItem(paper, event.x, event.y)
                            readerController.zoomAt(delta > 0 ? 1.12 : 1 / 1.12, point.x - 1, point.y - 1)
                            event.accepted = true
                        }
                    }
                    Column {
                        id: columnContent; width: 588
                        Repeater {
                            model: paper.columnPages
                            delegate: Item {
                                required property var modelData; width: 588; height: Number(modelData.columnHeight || 1); clip: true
                                Image { x: 0; y: -paper.columnInset; width: 588; height: 349; source: readingPage.visible ? modelData.source : ""; sourceSize: Qt.size(1260, 748); fillMode: Image.Stretch; smooth: true; mipmap: true; asynchronous: false }
                            }
                        }
                    }
                }
                NumberAnimation { id: verticalScroll; target: column; property: "contentY"; duration: 320; easing.type: Easing.OutCubic }

                Rectangle {
                    x: width - 24 - 16; y: 0; width: 16; height: 26; visible: !!readerPage.currentPage.bookmarked; color: "transparent"
                    Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: -1; fillColor: "#FFFA00"; PathSvg { path: "M0 0h16v26l-8-6.5L0 26z" } } }
                }
                MouseArea {
                    id: pagePointer; x: 1; y: 1; width: 588; height: 349; enabled: !paper.verticalMode
                    acceptedButtons: Qt.LeftButton; hoverEnabled: true
                    property real lastX: 0; property real lastY: 0
                    onPressed: mouse => { lastX = mouse.x; lastY = mouse.y }
                    onPositionChanged: mouse => { if (pressed && readerPage.readerState.zoom > 1) { readerController.panBy(mouse.x - lastX, mouse.y - lastY); lastX = mouse.x; lastY = mouse.y } }
                    onWheel: wheel => {
                        const modifier = wheel.modifiers & Qt.ControlModifier
                        const delta = wheel.angleDelta.y || wheel.pixelDelta.y
                        if (modifier && delta) readerController.zoomAt(delta > 0 ? 1.12 : 1 / 1.12, wheel.x, wheel.y)
                        else if (readerPage.readerState.zoom > 1) readerController.panBy(wheel.pixelDelta.x || wheel.angleDelta.x / 8, wheel.pixelDelta.y || wheel.angleDelta.y / 8)
                        else if (!paper.verticalMode && Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y) * 1.1) readerPage.turn(wheel.angleDelta.x > 0 ? 1 : -1)
                        wheel.accepted = true
                    }
                }
                PinchHandler {
                    id: pagePinch; target: null; enabled: !!readerPage.currentPage.key
                    minimumScale: 0.05; maximumScale: 12
                    property real lastScale: 1
                    onActiveChanged: if (active) lastScale = 1
                    onActiveScaleChanged: {
                        if (!active || activeScale <= 0) return
                        readerController.zoomAt(activeScale / lastScale, centroid.position.x - 1, centroid.position.y - 1)
                        lastScale = activeScale
                    }
                }
            }

            Item {
                x: 0; y: 478; width: 590; height: 30; visible: !!readerPage.currentPage.key
                IconButton { x: 0; icon: "left"; enabled: !!readerPage.currentPage.canPrevious; reduceMotion: readerPage.reduceMotion; onClicked: readerPage.turn(-1) }
                ReaderPageSlider { id: pageSlider; x: 38; width: 328; value: Number(readerPage.currentPage.number || 1); maximum: Math.max(2, Number(readerPage.currentPage.total || 100)); enabled: !!readerPage.currentPage.key; reduceMotion: readerPage.reduceMotion; onChanged: (value, final) => { if (final) readerController.jump(Math.round(value)) } }
                IconButton { x: 374; icon: "right"; enabled: !!readerPage.currentPage.canNext; reduceMotion: readerPage.reduceMotion; onClicked: readerPage.turn(1) }
                Text { x: 412; y: 10; width: 64; text: readerPage.currentPage.number ? readerPage.currentPage.number + " / " + readerPage.currentPage.total : ""; color: "#9A9A9A"; horizontalAlignment: Text.AlignRight; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; font.preferShaping: false; renderType: Text.CurveRendering }
                IconButton { x: 484; icon: "bookmark"; checked: !!readerPage.currentPage.bookmarked; reduceMotion: readerPage.reduceMotion; onClicked: readerController.toggleBookmark() }
                IconButton { objectName: "readerResetZoom"; x: 522; icon: "reset"; checked: readerPage.readerState.zoom > 1; enabled: readerPage.readerState.zoom > 1; reduceMotion: readerPage.reduceMotion; onClicked: readerController.resetZoom() }
                IconButton { x: 560; icon: "gear"; reduceMotion: readerPage.reduceMotion; onClicked: { readerPage.settingsOpen = !readerPage.settingsOpen; readerPage.fontMenuOpen = false } }
            }
        }

        Item {
            id: marksPage; objectName: "readerMarks"; visible: readerPage.tab === 3; x: 0; y: 125; width: 590; height: 385
            readonly property var marks: readerPage.currentBook.bookmarks || []
            ListView {
                anchors.fill: parent; model: marksPage.marks; spacing: 6; clip: true; boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    required property var modelData; required property int index
                    width: 590; height: 48; color: index % 2 ? "#1D1D1D" : "#222222"; border.color: "#3D3D3D"
                    Rectangle { width: 4; height: parent.height; color: "#FFFA00" }
                    Text { x: 16; y: 8; text: "书签 " + String(index + 1).padStart(2, "0"); color: "#EEEEEE"; font.family: harmonyBold.name; font.pixelSize: 13; font.weight: Font.Bold; renderType: Text.CurveRendering }
                    Text { x: 16; y: 28; text: "P. " + modelData.page; color: "#8A8A8A"; font.family: harmonyRegular.name; font.pixelSize: 9; renderType: Text.CurveRendering }
                    CalendarButton { x: 486; y: 11; width: 56; height: 26; title: "前往"; kind: "secondary"; reduceMotion: readerPage.reduceMotion; onClicked: { readerController.goBookmark(modelData.id); readerPage.selectTab(2, -1) } }
                    IconButton { x: 550; y: 9; icon: "x"; reduceMotion: readerPage.reduceMotion; onClicked: readerController.removeBookmark(modelData.id) }
                }
            }
            EmptyState { anchors.fill: parent; visible: marksPage.marks.length === 0; title: "NO BOOKMARKS"; subtitle: "" }
        }
    }

    Item {
        id: settings; objectName: "readerSettings"; x: 320; y: 302; width: 270; height: 168; z: 100
        visible: readerPage.settingsOpen && readerPage.tab === 2
        Rectangle { anchors.fill: parent; color: "#F51B1B1B"; border.color: "#5A5A5A" }
        Text { x: 12; y: 8; text: "阅读方向"; color: "#DDDDDD"; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
        Row {
            x: 96; y: 6
            Repeater {
                model: [{title: "从左到右", value: false}, {title: "从上到下", value: true}]
                delegate: Rectangle {
                    required property var modelData; width: 81; height: 26; color: readerPage.preferences.vertical === modelData.value ? "#FFFA00" : "#292929"; border.color: "#555555"
                    Text { anchors.centerIn: parent; text: parent.modelData.title; color: readerPage.preferences.vertical === parent.modelData.value ? "#171717" : "#EEEEEE"; font.family: harmonyMedium.name; font.pixelSize: 9; renderType: Text.CurveRendering }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: readerController.setPreference("vertical", parent.modelData.value) }
                }
            }
        }
        Text { x: 12; y: 48; text: "字体"; color: "#DDDDDD"; font.family: harmonyRegular.name; font.pointSize: 12.5 * 0.75; renderType: Text.CurveRendering }
        Rectangle {
            x: 108; y: 40; width: 150; height: 26; color: "#292929"; border.color: "#555555"
            Text { x: 8; width: 128; height: 26; verticalAlignment: Text.AlignVCenter; text: readerPage.preferences.fontName || "Georgia"; color: "#EEEEEE"; elide: Text.ElideRight; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
            Text { x: 136; y: 6; text: "⌄"; color: "#AAAAAA"; font.pixelSize: 11 }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (!readerPage.fontMenuOpen) readerPage.fonts = readerController.fontFamilies(); readerPage.fontMenuOpen = !readerPage.fontMenuOpen } }
        }
        Repeater {
            model: [
                { key: "fontSize", title: "字号", min: 10, max: 32, value: Number(readerPage.preferences.fontSize || 10) },
                { key: "lineSpacing", title: "行间距", min: 0, max: 18, value: Number(readerPage.preferences.lineSpacing || 0) },
                { key: "margin", title: "页边距", min: 6, max: 48, value: Number(readerPage.preferences.margin || 16) }
            ]
            delegate: Item {
                required property var modelData; required property int index; x: 12; y: 74 + index * 34; width: 246; height: 26
                Text { y: 6; text: parent.modelData.title; color: "#DDDDDD"; font.family: harmonyRegular.name; font.pointSize: 12.5 * 0.75; renderType: Text.CurveRendering }
                IconButton { x: 158; width: 26; height: 26; icon: "minus"; enabled: parent.modelData.value > parent.modelData.min; reduceMotion: readerPage.reduceMotion; onClicked: readerController.setPreference(parent.modelData.key, parent.modelData.value - 1) }
                Text { x: 184; y: 7; width: 36; text: Math.round(parent.modelData.value); color: "#EEEEEE"; horizontalAlignment: Text.AlignHCenter; font.family: harmonyBold.name; font.pixelSize: 12; font.weight: Font.Bold; renderType: Text.CurveRendering }
                IconButton { x: 220; width: 26; height: 26; icon: "plus"; enabled: parent.modelData.value < parent.modelData.max; reduceMotion: readerPage.reduceMotion; onClicked: readerController.setPreference(parent.modelData.key, parent.modelData.value + 1) }
            }
        }
    }

    Rectangle {
        x: 378; y: 180; width: 200; height: Math.min(260, fontList.contentHeight + 2); z: 180
        visible: readerPage.fontMenuOpen; color: "#FA1A1A1A"; border.color: "#5A5A5A"; clip: true
        ListView {
            id: fontList; anchors.fill: parent; model: readerPage.fonts; clip: true; boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property string modelData; width: 198; height: 28; color: fontHover.hovered ? "#343434" : modelData === readerPage.preferences.fontName ? "#282800" : "transparent"
                Text { x: 10; width: 178; height: 28; verticalAlignment: Text.AlignVCenter; text: parent.modelData; color: parent.modelData === readerPage.preferences.fontName ? "#FFFA00" : "#EEEEEE"; elide: Text.ElideRight; font.family: parent.modelData; font.pixelSize: 10; renderType: Text.CurveRendering }
                HoverHandler { id: fontHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { readerController.setPreference("fontName", parent.modelData); readerPage.fontMenuOpen = false } }
            }
            Controls.ScrollBar.vertical: Controls.ScrollBar { width: 4 }
        }
    }

    Item {
        anchors.fill: parent; z: 150; visible: readerPage.menuBookId.length > 0
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; onClicked: readerPage.menuBookId = "" }
        Rectangle {
            x: readerPage.menuPoint.x; y: readerPage.menuPoint.y; width: 170; height: 98; color: "#F51B1B1B"; border.color: "#555555"
            Column {
                Repeater {
                    model: [{title: "打开", action: "open"}, {title: "重新链接…", action: "relink"}, {title: "从书库移除", action: "remove"}]
                    delegate: Rectangle {
                        required property var modelData; required property int index; width: 168; height: 32; color: menuHover.hovered ? "#333333" : "transparent"
                        Rectangle { visible: index === 2; y: 0; width: parent.width; height: 1; color: "#444444" }
                        Text { x: 14; height: 32; verticalAlignment: Text.AlignVCenter; text: parent.modelData.title; color: index === 2 ? "#E35B5D" : "#EEEEEE"; font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                        HoverHandler { id: menuHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: {
                            const id = readerPage.menuBookId; const book = readerPage.books.find(value => value.id === id); readerPage.menuBookId = ""
                            if (parent.modelData.action === "open" && book) readerPage.openBook(book)
                            else if (parent.modelData.action === "relink") { readerPage.relinkBookId = id; bookDialog.open() }
                            else if (parent.modelData.action === "remove") readerPage.confirmBookId = id
                        } }
                    }
                }
            }
        }
    }

    Item {
        anchors.fill: parent; z: 160; visible: readerPage.shelfMenuOpen
        MouseArea { anchors.fill: parent; onClicked: readerPage.shelfMenuOpen = false }
        Rectangle {
            x: 352; y: 340; width: 220; height: Math.min(168, shelfList.contentHeight + 2); color: "#F51B1B1B"; border.color: "#555555"; clip: true
            ListView {
                id: shelfList; anchors.fill: parent; model: fileShelfController.readerChoices(); boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    required property var modelData; width: 218; height: 32; color: shelfHover.hovered ? "#333333" : "transparent"
                    Text { x: 12; width: 194; height: 32; verticalAlignment: Text.AlignVCenter; text: parent.modelData.title; color: "#EEEEEE"; elide: Text.ElideRight; font.family: harmonyMedium.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                    HoverHandler { id: shelfHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { const url = fileShelfController.readerUrl(parent.modelData.id); readerPage.shelfMenuOpen = false; if (url) readerController.importUrl(url) } }
                }
            }
        }
    }

    Item {
        anchors.fill: parent; z: 210; visible: readerPage.confirmBookId.length > 0
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: readerPage.confirmBookId = "" }
        ClipboardConfirmBar {
            x: -15; y: 370
            title: { const book = readerPage.books.find(value => value.id === readerPage.confirmBookId); return "从书库移除 " + (book ? book.title : "书籍") + "？" }
            message: "文件本身仍保留在磁盘上。"; confirmTitle: "移除"; reduceMotion: readerPage.reduceMotion
            onCancelled: readerPage.confirmBookId = ""
            onAccepted: { const id = readerPage.confirmBookId; readerPage.confirmBookId = ""; readerController.removeBook(id) }
        }
    }

    Item {
        x: 320; y: 424; width: 270; height: 58; z: 300; opacity: readerPage.toastOpen ? 1 : 0; visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: readerPage.reduceMotion ? 0 : 250 } }
        Rectangle { anchors.fill: parent; color: "#F0181818"; border.color: "#505050" }
        Rectangle { width: 4; height: parent.height; color: "#FFFA00" }
        Text { x: 18; y: 10; width: 240; text: readerPage.toastTitle; color: "#F2F2F2"; elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 18; y: 33; width: 240; text: readerPage.toastMessage; color: "#A0A0A0"; elide: Text.ElideRight; font.family: harmonyRegular.name; font.pixelSize: 9; renderType: Text.CurveRendering }
        Timer { id: toastTimer; interval: 2600; onTriggered: readerPage.toastOpen = false }
    }

    Shortcut { sequence: "Q"; enabled: readerPage.presented && !bookDialog.visible; onActivated: readerPage.cycleTab(-1) }
    Shortcut { sequence: "E"; enabled: readerPage.presented && !bookDialog.visible; onActivated: readerPage.cycleTab(1) }
    Shortcut { sequence: "Left"; enabled: readerPage.presented && readerPage.tab === 2; onActivated: readerPage.turn(-1) }
    Shortcut { sequence: "Right"; enabled: readerPage.presented && readerPage.tab === 2; onActivated: readerPage.turn(1) }
    Shortcut { sequence: "Up"; enabled: readerPage.presented && readerPage.tab === 2 && readerPage.preferences.vertical; onActivated: readerPage.turn(-1) }
    Shortcut { sequence: "Down"; enabled: readerPage.presented && readerPage.tab === 2 && readerPage.preferences.vertical; onActivated: readerPage.turn(1) }
    Shortcut { sequence: "Space"; enabled: readerPage.presented && readerPage.tab === 2; onActivated: readerPage.turn(1) }
    Shortcut { sequence: "Escape"; enabled: readerPage.presented && readerPage.tab === 2 && readerPage.readerState.zoom > 1; onActivated: readerController.resetZoom() }
}
