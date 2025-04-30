package com.tal.xes.bookpage.func

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Path
import com.tal.xes.bookpage.data.Line
import com.tal.xes.bookpage.data.Point
import kotlin.math.PI
import kotlin.math.pow
import kotlin.math.sqrt

/**
 * This function is used to calculate the distance between two points in 2D space.
 */
internal fun Point.distanceTo(other: Point) = sqrt((this.x - other.x).pow(2) + (this.y - other.y).pow(2))

/**
 * This function is used to calculate the distance from a point to a line in 2D space.
 */
internal fun Point.distanceTo(line: Line) = (line.k * this.x - this.y + line.b) / sqrt(line.k.pow(2) + 1)

/**
 * This function is used to check if a point is below a line in 2D space.
 */
internal fun Point.isBelowLine(line: Line) = line.k * this.x + line.b > this.y

/**
 * This function is used to check if a point is above a line in 2D space.
 */
internal fun Point.isAboveLine(line: Line) = line.k * this.x + line.b < this.y

/**
 * This function is used to check if a point is on a line in 2D space.
 */
internal fun Point.isOnLine(line: Line) = line.k * this.x + line.b == this.y

/**
 * This function is used to get the symmetrical point of a point with respect to a line in 2D space.
 */
internal fun Point.getSymmetricalPoint(line: Line) = with(line){
    if(k == 0f) {
        Point(x, b * 2 - y)
    } else {
        val x1 = x - 2 * k.pow(2) * x + 2 * k * (y - b) / (1 + k.pow(2))
        val y1 = -y + 2 * (k * x + b) / (1 + k.pow(2))
        Point(x1, y1)
    }
}

/**
 * This function is used to convert an Offset object to a Point object.
 */
internal val Offset.toPoint get() = Point(x, y)
/**
 * This function is used to convert a Point object to an Offset object.
 */
internal val Point.toOffset get() = Offset(x, y)

/**
 * This function is used to move a path to a specified point.
 */
internal fun Path.moveTo(point: Point) = moveTo(point.x, point.y)
/**
 * This function is used to add a line to a path from the current point to a specified point.
 */
internal fun Path.lineTo(point: Point) = lineTo(point.x, point.y)

/**
 * This function is used to add a quadratic bezier curve to a path from the current point to a specified point with a control point.
 */
internal fun Path.quadraticBezierTo(
    controlPoint: Point,
    endPoint: Point
) = quadraticBezierTo(controlPoint.x, controlPoint.y, endPoint.x, endPoint.y)

/**
 * This function is used to connect a path to a list of points with a specified down sampling factor.
 */
internal fun Path.connect(points: List<Float>, distortedEdgeDownSampling: Int, reverse: Boolean = false) {
    val size = points.size / 2
    val downSampling = maxOf(size / distortedEdgeDownSampling, 1)

    (0 until size step downSampling).forEach { i ->
        val xIndex = i + i
        if (!reverse) {
            lineTo(points[xIndex], points[xIndex + 1])
        } else {
            val rXIndex = size + size - xIndex - 2
            lineTo(points[rXIndex], points[rXIndex + 1])
        }
    }
}

/**
 * This function is used to check if a point is NaN and return a default point if it is.
 */
internal fun Point.avoidNaN() = if (x.isNaN() || y.isNaN()) Point(0f, 0f) else this

private fun Float.toRad() = ((this * PI) / 180).toFloat()
private fun Float.toDeg() = (this * 180 / PI).toFloat()