package com.tal.xes.bookpage.data

import kotlin.math.atan
import kotlin.math.tan


/**
 * AllPoints class representing a collection of points in 2D space.
 * @param O The origin point.
 * @param A The bottom & left point of page.
 * @param B The top & right point of page.
 * @param C The bottom & right point of page.
 **/
internal data class AllPoints(
    val O: Point,
    val A: Point,
    val B: Point,
    val C: Point,
    val H: Point,
    val I: Point,
    val J: Point,
    val M: Point,
    val N: Point,
    val S: Point,
    val T: Point,
    val U: Point,
    val V: Point,
    val W: Point,
    val Z: Point,
) {
    fun toAbsoluteSystem() = AllPoints(
        O.toAbsoluteSystem(),
        A.toAbsoluteSystem(),
        B.toAbsoluteSystem(),
        C.toAbsoluteSystem(),
        H.toAbsoluteSystem(),
        I.toAbsoluteSystem(),
        J.toAbsoluteSystem(),
        M.toAbsoluteSystem(),
        N.toAbsoluteSystem(),
        S.toAbsoluteSystem(),
        T.toAbsoluteSystem(),
        U.toAbsoluteSystem(),
        V.toAbsoluteSystem(),
        W.toAbsoluteSystem(),
        Z.toAbsoluteSystem()
    )

    companion object {
        fun default(absO: Point) = AllPoints(
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy(),
            absO.copy()
        )
    }

    fun toCartesianSystem() = AllPoints(
        O,
        A.toCartesianSystem(),
        B.toCartesianSystem(),
        C.toCartesianSystem(),
        H.toCartesianSystem(),
        I.toCartesianSystem(),
        J.toCartesianSystem(),
        M.toCartesianSystem(),
        N.toCartesianSystem(),
        S.toCartesianSystem(),
        T.toCartesianSystem(),
        U.toCartesianSystem(),
        V.toCartesianSystem(),
        W.toCartesianSystem(),
        Z.toCartesianSystem()
    )
}

internal data class Line(val k: Float, val b: Float) {
    /**
     * Calculate the y-coordinate for a given x-coordinate using the line equation.
     *
     * @param x The x-coordinate.
     * @return The y-coordinate.
     */
    fun getY(x: Float): Float = k * x + b

    /**
     * Calculate the x-coordinate for a given y-coordinate using the line equation.
     *
     * @param y The y-coordinate.
     * @return The x-coordinate.
     */
    fun getX(y: Float): Float = (y - b) / k

    /**
     * Calculate the intersection point of this line with another line.
     *
     * @param other The other line.
     * @return The intersection point.
     */
    fun intersection(other: Line): Point {
        val x = (other.b - b) / (k - other.k)
        return Point(x, getY(x))
    }

    /**
     * Calculate the angle (in radians) of this line.
     *
     * @return The angle in radians.
     */
    fun theta() = theta(k)

    companion object{

        /**
         * Calculate the angle (in radians) of a line given its slope.
         *
         * @param k The slope of the line.
         * @return The angle in radians.
         */
        fun withTwoPoints(p1: Point, p2: Point): Line {
            val k = (p2.y - p1.y) / (p2.x - p1.x)
            val b = p1.y - k * p1.x
            return Line(k, b)
        }

        /**
         * Create a line using a slope and a point.
         *
         * @param k The slope of the line.
         * @param p1 A point on the line.
         * @return The line defined by the slope and the point.
         */
        fun withKAndOnePoint(k: Float, p1: Point): Line {
            val b = p1.y - k * p1.x
            return Line(k, b)
        }

        /**
         * Calculate the angle (in radians) of a line given its slope.
         *
         * @param k The slope of the line.
         * @return The angle in radians.
         */
        fun theta(k: Float): Float = when {
                k > 0 -> atan(k.toDouble()).toFloat()
                k < 0 -> atan(k.toDouble()).toFloat() + Math.PI.toFloat()
                else -> 0f
            }

        /**
         * Calculate the slope of a line given its angle (in radians).
         *
         * @param theta The angle in radians.
         * @return The slope of the line.
         */
        fun k(theta: Float): Float = when {
                theta > 0 -> tan(theta.toDouble()).toFloat()
                theta < 0 -> tan(theta.toDouble()).toFloat()
                else -> 0f
            }
    }
}

/**
 * Point class representing a point in 2D space.
 *
 * @property x The x-coordinate of the point.
 * @property y The y-coordinate of the point.
 */
internal data class Point(
    val x: Float,
    val y: Float,
) {
    /**
     * plus with another point
     */
    operator fun plus(other: Point) = Point(x + other.x, y + other.y)

    /**
     * minus with another point
     */
    operator fun minus(other: Point) = Point(x - other.x, y - other.y)

    /**
     * multiply by a number
     */
    operator fun times(number: Float) = Point(x * number, y * number)

    /**
     * divide by a number
     */
    operator fun div(number: Float) = Point(x / number, y / number)

    /**
     * Get the middle point between this point and another point.
     */
    operator fun rangeTo(other: Point) = Point(0.5f * (this.x + other.x), 0.5f * (this.y + other.y))

    /**
     * Convert to absolute system
     */
    fun toAbsoluteSystem() = Point(x, -y)

    /**
     * Convert to cartesian system
     */
    fun toCartesianSystem() = Point(x, -y)

    companion object{
        val ZERO = Point(0f, 0f)
    }
}