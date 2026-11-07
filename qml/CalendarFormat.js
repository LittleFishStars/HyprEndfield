.pragma library

function pad2(value) { return String(value).padStart(2, "0") }
function key(year, month, day) { return String(year).padStart(4, "0") + "-" + pad2(month) + "-" + pad2(day) }
function parts(value) {
    const fields = String(value).split("-")
    return { year: Number(fields[0]), month: Number(fields[1]), day: Number(fields[2]) }
}
function daysInMonth(year, month) { return new Date(year, month, 0, 12).getDate() }
function monthName(year, month) { return year + "年" + month + "月" }
function weekdayName(year, month, day) {
    return ["星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"][new Date(year, month - 1, day, 12).getDay()]
}
function cells(year, month) {
    const first = new Date(year, month - 1, 1, 12)
    const padding = first.getDay() // EndfieldHUD source fixture: Sunday is firstWeekday 1.
    const result = []
    for (let index = 0; index < 42; ++index) {
        const date = new Date(year, month - 1, 1 - padding + index, 12)
        result.push({
            year: date.getFullYear(), month: date.getMonth() + 1, day: date.getDate(),
            key: key(date.getFullYear(), date.getMonth() + 1, date.getDate()),
            outside: date.getMonth() + 1 !== month,
            weekend: date.getDay() === 0 || date.getDay() === 6
        })
    }
    return result
}
