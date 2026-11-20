.pragma library

// View-only plan-camera framing. X points right and Z points down on screen;
// all dimensions are schematic layout coordinates, not hydraulic geometry.
function fitPlan(bounds, viewport, insets, zoom) {
    if (!bounds || !viewport || !insets) return null
    const values = [bounds.minX, bounds.maxX, bounds.minZ, bounds.maxZ,
        viewport.width, viewport.height, insets.left, insets.right, insets.top, insets.bottom, zoom]
    if (!values.every(Number.isFinite) || bounds.minX > bounds.maxX || bounds.minZ > bounds.maxZ
            || viewport.width <= 0 || viewport.height <= 0 || zoom <= 0
            || [insets.left, insets.right, insets.top, insets.bottom].some(value => value < 0)) return null

    const width = viewport.width - insets.left - insets.right
    const height = viewport.height - insets.top - insets.bottom
    const extentX = bounds.maxX - bounds.minX, extentZ = bounds.maxZ - bounds.minZ
    const spanX = Math.max(180, extentX + 180), spanZ = Math.max(180, extentZ + 180)
    if (![width, height, spanX, spanZ].every(Number.isFinite) || width <= 0 || height <= 0) return null

    const scaled = Math.min(width / spanX, height / spanZ) * zoom
    if (!Number.isFinite(scaled)) return null
    const magnification = Math.max(1e-6, scaled)
    const centerX = bounds.minX + extentX / 2, centerZ = bounds.minZ + extentZ / 2
    const cameraX = centerX - (insets.left + width / 2 - viewport.width / 2) / magnification
    const cameraZ = centerZ - (insets.top + height / 2 - viewport.height / 2) / magnification
    if (![cameraX, cameraZ].every(Number.isFinite)) return null
    return {magnification: magnification, cameraX: cameraX, cameraZ: cameraZ,
        usable: {x: insets.left, y: insets.top, width: width, height: height}, spanX: spanX, spanZ: spanZ}
}
