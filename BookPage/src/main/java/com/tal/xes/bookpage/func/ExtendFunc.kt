package com.tal.xes.bookpage.func

import com.tal.xes.bookpage.data.Line
import com.tal.xes.bookpage.data.Point
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