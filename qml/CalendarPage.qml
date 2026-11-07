import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import "Appearance.js" as Appearance
import QtQuick.Shapes
import "CalendarFormat.js" as Format

// EndfieldHUD HUDCalendarCanvas, MOD-12.
Item {
    id: calendarPage
    width: 590; height: 510; focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property var calendar: calendarController.state
    readonly property var events: calendar.events
    property int shownYear: 2000
    property int shownMonth: 1
    property string selectedDay: ""
    property string editingId: ""
    property string draftTitle: ""
    property string draftDetails: ""
    property string motionPhase: "idle"
    property int direction: 1
    property var pendingMonth: null
    property string toastTitle: ""
    property string toastDetail: ""
    property bool toastOpen: false
    property string confirmTarget: ""
    property string lastError: ""
    readonly property var monthCells: Format.cells(shownYear, shownMonth)
    readonly property var selectedEvents: events.filter(event => event.day === selectedDay)
    readonly property real eventsHeight: Math.max(0, eventStack.height)
    readonly property real listHeight: Math.min(150, Math.max(40, eventsHeight))
    readonly property real fieldTop: 55 + listHeight + 6

    function initialise() {
        const today = Format.parts(calendar.today)
        shownYear = today.year; shownMonth = today.month; selectedDay = calendar.today
    }
    function showToast(title, detail) {
        toastTitle = title; toastDetail = detail || ""; toastOpen = true; toastTimer.restart()
    }
    function hasEvent(day) { return events.some(event => event.day === day) }
    function eventById(id) { return events.find(event => event.id === id) }
    function stopEditing() {
        if (!editingId) return
        editingId = ""; titleField.text = draftTitle; detailsField.text = draftDetails; titleField.invalid = false
    }
    function edit(entry) {
        if (editingId === entry.id) { stopEditing(); return }
        if (!editingId) { draftTitle = titleField.text; draftDetails = detailsField.text }
        editingId = entry.id; titleField.text = entry.title; detailsField.text = entry.details; titleField.invalid = false
    }
    function chooseDay(day) { selectedDay = day; stopEditing(); rootFocus.forceActiveFocus() }
    function applyMonth(year, month, day) {
        shownYear = year; shownMonth = month; selectedDay = day; stopEditing()
    }
    function navigateTo(year, month, day, directionHint) {
        direction = directionHint
        if (reduceMotion) { applyMonth(year, month, day); motionPhase = "idle"; return }
        pendingMonth = { year: year, month: month, day: day }; motionPhase = "leave"; swap.restart()
    }
    function changeMonth(step) {
        if (motionPhase !== "idle") return
        picker.visible = false
        const date = new Date(shownYear, shownMonth - 1 + step, 1, 12)
        if (date.getFullYear() < 1900 || date.getFullYear() > 9999) return
        navigateTo(date.getFullYear(), date.getMonth() + 1, Format.key(date.getFullYear(), date.getMonth() + 1, 1), step)
    }
    function goToday() {
        picker.visible = false
        const today = Format.parts(calendar.today)
        if (today.year === shownYear && today.month === shownMonth) { selectedDay = calendar.today; stopEditing(); return }
        navigateTo(today.year, today.month, calendar.today,
                   today.year * 12 + today.month > shownYear * 12 + shownMonth ? 1 : -1)
    }
    function pickMonth(year, month) {
        picker.visible = false
        const today = Format.parts(calendar.today)
        const day = today.year === year && today.month === month ? calendar.today : Format.key(year, month, 1)
        if (year === shownYear && month === shownMonth) { chooseDay(day); return }
        navigateTo(year, month, day, year * 12 + month > shownYear * 12 + shownMonth ? 1 : -1)
    }
    function moveDay(step) {
        const selected = Format.parts(selectedDay)
        const target = Math.max(1, Math.min(Format.daysInMonth(shownYear, shownMonth), selected.day + step))
        if (target !== selected.day) chooseDay(Format.key(shownYear, shownMonth, target))
    }
    function addEvent() {
        const title = titleField.text.trim()
        if (!title) { titleField.invalid = true; titleField.edit(); return }
        if (calendarController.saveEvent(title, detailsField.text, selectedDay, "")) {
            draftTitle = ""; draftDetails = ""; titleField.text = ""; detailsField.text = ""
            showToast(qsTr("事项已添加"), title)
        }
    }
    function saveEvent() {
        const entry = eventById(editingId); if (!entry) return
        const title = titleField.text.trim() || entry.title
        if (calendarController.saveEvent(title, detailsField.text, entry.day, entry.id)) {
            stopEditing(); showToast(qsTr("事项已保存"), title)
        }
    }
    function requestDelete() {
        const entry = eventById(editingId); if (!entry) return
        confirmTarget = entry.id; confirm.message = entry.title; confirmLayer.visible = true
    }
    function finishDelete() {
        const entry = eventById(confirmTarget)
        if (calendarController.removeEvent(confirmTarget)) {
            if (editingId === confirmTarget) stopEditing()
            showToast(qsTr("提醒已取消"), entry ? entry.title : "")
        }
        confirmTarget = ""; confirmLayer.visible = false
    }

    Component.onCompleted: initialise()
    onPresentedChanged: { calendarController.active = presented; if (presented) rootFocus.forceActiveFocus(); else { picker.visible = false; confirmLayer.visible = false } }
    Component.onDestruction: calendarController.active = false
    Connections {
        target: calendarController
        function onStateChanged() {
            if (calendarPage.calendar.error && calendarPage.calendar.error !== calendarPage.lastError) {
                calendarPage.lastError = calendarPage.calendar.error
                calendarPage.showToast(qsTr("日历"), calendarPage.calendar.error)
            }
            if (calendarPage.editingId && !calendarPage.eventById(calendarPage.editingId)) calendarPage.stopEditing()
        }
    }
    Item { id: rootFocus; anchors.fill: parent; focus: true }
    Timer { id: swap; interval: 130; onTriggered: { const next = calendarPage.pendingMonth; calendarPage.applyMonth(next.year, next.month, next.day); calendarPage.motionPhase = "enter"; settle.restart() } }
    Timer { id: settle; interval: 476; onTriggered: calendarPage.motionPhase = "idle" }
    Timer { id: toastTimer; interval: 1500; onTriggered: calendarPage.toastOpen = false }
    Timer { id: deleteDelay; interval: calendarPage.reduceMotion ? 0 : 120; onTriggered: calendarPage.finishDelete() }
    Shortcut { sequence: "Left"; enabled: calendarPage.presented && !titleField.fieldFocused && !detailsField.fieldFocused && !picker.visible && !confirmLayer.visible; onActivated: calendarPage.moveDay(-1) }
    Shortcut { sequence: "Right"; enabled: calendarPage.presented && !titleField.fieldFocused && !detailsField.fieldFocused && !picker.visible && !confirmLayer.visible; onActivated: calendarPage.moveDay(1) }
    Shortcut { sequence: "Escape"; enabled: calendarPage.presented && (picker.visible || confirmLayer.visible); onActivated: { picker.visible = false; confirmLayer.visible = false } }
    Shortcut { sequence: "Return"; enabled: calendarPage.presented && confirmLayer.visible; onActivated: { confirmLayer.visible = false; deleteDelay.restart() } }

    Item {
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "CALENDAR"]
                Text {
                    required property string modelData; required property int index
                    text: modelData; color: index === 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A")
                    font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
                }
            }
        }
        Text {
            x: 549 - width; y: 1.75; text: ModuleNumbering.code("calendar"); color: Appearance.tone("#8A8A8A")
            font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
        }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p } } }
            }
        }
        Text {
            id: title; y: 20; height: 30; text: qsTr("日历"); color: Appearance.tone("#F4F4F4")
            font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08; renderType: Text.CurveRendering
        }
        Text {
            x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset; text: "CALENDAR"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium; renderType: Text.CurveRendering
        }
        CalendarButton { x: 497; y: 22; width: 55; title: qsTr("今天"); kind: "secondary"; reduceMotion: calendarPage.reduceMotion; onClicked: calendarPage.goToday() }
        CalendarButton {
            x: 560; y: 20; width: 30; kind: "icon"; glyph: "↻"; reduceMotion: calendarPage.reduceMotion
            onClicked: calendarPage.showToast(calendarController.refreshReminders() ? qsTr("提醒已安排") : qsTr("提醒已关闭"), calendarController.state.reminderStatus)
        }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - width; y: -2; width: counter.width + 7; height: 12; color: Appearance.tone("#151515") }
            Text {
                id: counter; x: 590 - width; y: -1.5; text: String(calendarPage.calendar.total).padStart(2, "0") + qsTr(" 个事项")
                color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering
            }
        }
    }

    NowPlayingSection {
        page: calendarPage; sectionIndex: 0; x: 0; y: 81; width: 322; height: 314
        CalendarButton { width: 30; kind: "icon"; glyph: "‹"; enabled: calendarPage.shownYear > 1900 || calendarPage.shownMonth > 1; reduceMotion: calendarPage.reduceMotion; onClicked: calendarPage.changeMonth(-1) }
        Item {
            id: monthTitle; x: 41; width: 240; height: 30; transform: Translate { y: monthHover.hovered ? -1 : 0 }
            Rectangle { anchors.fill: parent; color: "white"; opacity: monthHover.hovered ? 0.06 : 0 }
            Text {
                anchors.centerIn: parent; anchors.horizontalCenterOffset: -7; text: Format.monthName(calendarPage.shownYear, calendarPage.shownMonth); color: Appearance.tone("#EDEDED")
                font.family: harmonyMedium.name; font.pixelSize: 17; font.weight: Font.Medium; font.letterSpacing: 0.68; renderType: Text.CurveRendering
            }
            Text { anchors.centerIn: parent; anchors.horizontalCenterOffset: 73; text: "▾"; color: monthHover.hovered ? Appearance.tone("#EDEDED") : Appearance.tone("#8A8A8A"); font.pixelSize: 12 }
            HoverHandler { id: monthHover }
            MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { picker.year = calendarPage.shownYear; picker.visible = !picker.visible } }
        }
        CalendarButton { x: 292; width: 30; kind: "icon"; glyph: "›"; enabled: calendarPage.shownYear < 9999 || calendarPage.shownMonth < 12; reduceMotion: calendarPage.reduceMotion; onClicked: calendarPage.changeMonth(1) }
        Row {
            y: 39.5; spacing: 4
            Repeater {
                model: [qsTr("周日"), qsTr("周一"), qsTr("周二"), qsTr("周三"), qsTr("周四"), qsTr("周五"), qsTr("周六")]
                Text {
                    required property string modelData; width: 42.57; height: 10; horizontalAlignment: Text.AlignHCenter
                    text: modelData; color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 9.5 * 0.75; font.letterSpacing: 0.95; renderType: Text.CurveRendering
                }
            }
        }
        Grid {
            y: 54; columns: 7; columnSpacing: 4; rowSpacing: 4
            Repeater {
                model: calendarPage.monthCells
                CalendarDayCell {
                    required property var modelData
                    value: modelData.day; outside: modelData.outside; weekend: modelData.weekend
                    event: calendarPage.hasEvent(modelData.key); today: modelData.key === calendarPage.calendar.today
                    selected: modelData.key === calendarPage.selectedDay; reduceMotion: calendarPage.reduceMotion
                    onClicked: calendarPage.chooseDay(modelData.key)
                }
            }
        }
    }

    NowPlayingSection {
        page: calendarPage; sectionIndex: 1; x: 336; y: 81; width: 254; height: 314
        readonly property var selectedParts: Format.parts(calendarPage.selectedDay)
        Text {
            id: dayNumber; width: implicitWidth + 1; height: 49; text: Format.pad2(parent.selectedParts.day); color: Appearance.tone("#F4F4F4")
            font.family: harmonyBold.name; font.pixelSize: 54; font.weight: Font.Bold; font.features: { "tnum": 1 }; renderType: Text.CurveRendering
        }
        Text {
            x: dayNumber.width + 10; y: 33; width: 254 - x; height: 10; text: Format.weekdayName(parent.selectedParts.year, parent.selectedParts.month, parent.selectedParts.day)
            color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pixelSize: 10; font.letterSpacing: 1; renderType: Text.CurveRendering
        }
        Flickable {
            id: eventList; objectName: "calendarEvents"; x: 0; y: 55; width: 254; height: calendarPage.listHeight; contentWidth: width; contentHeight: eventStack.height; clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: eventStack; width: 248; spacing: 6
                Repeater {
                    model: calendarPage.selectedEvents
                    CalendarEventPlate {
                        required property var modelData; entry: modelData; selected: calendarPage.editingId === modelData.id; reduceMotion: calendarPage.reduceMotion
                        onClicked: calendarPage.edit(modelData)
                    }
                }
            }
            Text {
                y: 10; width: 254; height: 10; visible: calendarPage.selectedEvents.length === 0; text: qsTr("暂无事项"); color: Appearance.tone("#8A8A8A")
                font.family: harmonyRegular.name; font.pixelSize: 10; font.letterSpacing: 1; renderType: Text.CurveRendering
            }
            Rectangle {
                x: 250; width: 4; visible: eventList.contentHeight > eventList.height; color: Appearance.tone("#222222"); height: eventList.height
                Rectangle {
                    width: 4; color: Appearance.accent("#FFFA00"); height: Math.max(18, eventList.height * eventList.height / eventList.contentHeight)
                    y: eventList.contentHeight > eventList.height ? eventList.contentY / (eventList.contentHeight - eventList.height) * (eventList.height - height) : 0
                }
            }
        }
        CalendarTextField {
            id: titleField; objectName: "calendarTitle"; y: calendarPage.fieldTop; placeholder: calendarPage.editingId ? qsTr("标题") : qsTr("新事项"); maximumLength: 120; reduceMotion: calendarPage.reduceMotion
            onAccepted: calendarPage.editingId ? calendarPage.saveEvent() : calendarPage.addEvent()
        }
        CalendarTextField {
            id: detailsField; objectName: "calendarDetails"; y: calendarPage.fieldTop + 38; placeholder: qsTr("详情"); maximumLength: 2000; reduceMotion: calendarPage.reduceMotion
            onAccepted: calendarPage.editingId ? calendarPage.saveEvent() : calendarPage.addEvent()
        }
        CalendarButton {
            y: calendarPage.fieldTop + 76; width: calendarPage.editingId ? 177 : 254; title: calendarPage.editingId ? qsTr("保存") : qsTr("添加事项"); kind: "primary"; reduceMotion: calendarPage.reduceMotion
            onClicked: calendarPage.editingId ? calendarPage.saveEvent() : calendarPage.addEvent()
        }
        CalendarButton {
            x: 183; y: calendarPage.fieldTop + 76; width: 71; visible: calendarPage.editingId !== ""; title: qsTr("删除"); kind: "secondary"; reduceMotion: calendarPage.reduceMotion
            onClicked: calendarPage.requestDelete()
        }
    }

    Item {
        anchors.fill: parent; z: 99; visible: picker.visible
        MouseArea { anchors.fill: parent; onClicked: picker.visible = false }
    }
    CalendarMonthPicker {
        id: picker; objectName: "calendarMonthPicker"; x: 26; y: 117; z: 100; visible: false; shownYear: calendarPage.shownYear; shownMonth: calendarPage.shownMonth
        today: calendarPage.calendar.today; reduceMotion: calendarPage.reduceMotion
        onPicked: (year, month) => calendarPage.pickMonth(year, month)
    }
    Item {
        id: confirmLayer; objectName: "calendarConfirm"; anchors.fill: parent; z: 200; visible: false
        Rectangle { anchors.fill: parent; color: "#99000000"; Behavior on opacity { NumberAnimation { duration: calendarPage.reduceMotion ? 0 : 220 } } }
        MouseArea { anchors.fill: parent; onClicked: confirmLayer.visible = false }
        CalendarConfirmBar {
            id: confirm; x: -15; y: 370; reduceMotion: calendarPage.reduceMotion
            onCancelled: confirmLayer.visible = false
            onAccepted: { confirmLayer.visible = false; deleteDelay.restart() }
        }
    }
    Item {
        x: 320; y: 424; width: 270; height: 58; z: 300; opacity: calendarPage.toastOpen ? 1 : 0; visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: calendarPage.reduceMotion ? 0 : 250 } }
        Rectangle { anchors.fill: parent; color: "#F0181818"; border.color: Appearance.tone("#505050") }
        Rectangle { width: 4; height: parent.height; color: Appearance.accent("#FFFA00") }
        Text { x: 18; y: 10; width: 240; text: calendarPage.toastTitle; color: Appearance.tone("#F2F2F2"); elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 18; y: 33; width: 240; text: calendarPage.toastDetail; color: Appearance.tone("#A0A0A0"); elide: Text.ElideRight; font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering }
    }
}
