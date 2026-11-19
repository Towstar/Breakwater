.pragma library

// View-only screen rectangles. Inputs are never changed, and identity/order is
// stable for a fixed camera. Equipment bodies remain independently pickable.
function place(candidates, bounds, exclusions) {
    if (![bounds.x, bounds.y, bounds.width, bounds.height].every(Number.isFinite)
            || bounds.width <= 0 || bounds.height <= 0) return []
    const occupied = (exclusions || []).slice(), result = []
    const ordered = candidates.filter(row => row && typeof row.id === "string"
        && [row.anchorX, row.anchorY, row.width, row.height, row.priority].every(Number.isFinite)
        && row.width > 0 && row.height > 0 && row.width <= bounds.width && row.height <= bounds.height)
        .slice().sort((a, b) => a.priority - b.priority || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))
    function overlaps(a, b) {
        return a.x < b.x + b.width + 4 && a.x + a.width + 4 > b.x
            && a.y < b.y + b.height + 4 && a.y + a.height + 4 > b.y
    }
    for (const row of ordered) {
        const attempts = [
            [row.anchorX - row.width / 2, row.anchorY],
            [row.anchorX - row.width / 2, row.anchorY - row.height - 6],
            [row.anchorX + 8, row.anchorY],
            [row.anchorX - row.width - 8, row.anchorY],
            [row.anchorX - row.width / 2, row.anchorY + row.height + 6],
            [row.anchorX - row.width / 2, row.anchorY - 2 * (row.height + 6)]
        ]
        for (const attempt of attempts) {
            const rectangle = {x: Math.max(bounds.x, Math.min(bounds.x + bounds.width - row.width, attempt[0])),
                y: Math.max(bounds.y, Math.min(bounds.y + bounds.height - row.height, attempt[1])), width: row.width, height: row.height}
            if (occupied.some(other => overlaps(rectangle, other))) continue
            result.push(Object.assign({}, row, rectangle)); occupied.push(rectangle); break
        }
    }
    return result
}
