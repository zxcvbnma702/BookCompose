package com.tal.xes.bookpage.data

import com.tal.xes.bookpage.func.isAboveLine
import com.tal.xes.bookpage.func.isBelowLine
import com.tal.xes.bookpage.func.isOnLine

internal enum class DragDirection{
    Right,
    RightUp,
    Up,
    LeftUp,
    Left,
    LeftDown,
    Down,
    RightDown,
    Static
}

/**
 * DragEvent class representing a drag event with origin and current touch points.
 * @param originTouchPoint The initial touch point.
 * @param currentTouchPoint The current touch point.
 */
internal data class DragEvent(val originTouchPoint: Point, val currentTouchPoint: Point) {
    // 计算拖动偏移
    val dragDelta get() = currentTouchPoint - originTouchPoint

    /**
     * 将当前触摸点转换为笛卡尔坐标系，然后返回一个新的拖动事件
     * @return 新的拖动事件，使用笛卡尔坐标系中的点
     */
    fun toCartesianSystem(): DragEvent {
        val newOrigin = originTouchPoint.toCartesianSystem()
        val newCurrent = currentTouchPoint.toCartesianSystem()
        return DragEvent(newOrigin, newCurrent)
    }

    // 判断拖动是否发生移动
    val isUnmoved get() = currentTouchPoint.x == originTouchPoint.x && currentTouchPoint.y == originTouchPoint.y

    /**
     * 根据给定的线来计算拖动方向
     * @param line 拖动参考的线
     * @return 拖动方向
     */
    fun directionToLineInCartesianSystem(line: Line): DragDirection = when {
        line.k == 0f -> directionToOInCartesianSystem() // 如果线是水平的，使用原点方向计算
        line.k > 0f -> { // 如果线斜率大于零，即线倾斜向右
            val lParallel = Line.withKAndOnePoint(line.k, originTouchPoint)
            val lVertical = Line.withKAndOnePoint(-1 / line.k, originTouchPoint)
            currentTouchPoint.run {
                when {
                    isOnLine(lParallel)    && isAboveLine(lVertical) -> DragDirection.Right
                    isAboveLine(lParallel) && isAboveLine(lVertical) -> DragDirection.RightUp
                    isAboveLine(lParallel) && isOnLine(lVertical)    -> DragDirection.Up
                    isAboveLine(lParallel) && isBelowLine(lVertical) -> DragDirection.LeftUp
                    isOnLine(lParallel)    && isBelowLine(lVertical) -> DragDirection.Left
                    isBelowLine(lParallel) && isBelowLine(lVertical) -> DragDirection.LeftDown
                    isBelowLine(lParallel) && isOnLine(lVertical)    -> DragDirection.Down
                    isBelowLine(lParallel) && isAboveLine(lVertical) -> DragDirection.RightDown
                    else -> DragDirection.Static
                }
            }
        }
        line.k < 0f -> { // 如果线斜率小于零，即线倾斜向左
            val lParallel = Line.withKAndOnePoint(line.k, originTouchPoint)
            val lVertical = Line.withKAndOnePoint(-1 / line.k, originTouchPoint)
            currentTouchPoint.run {
                when {
                    isOnLine(lParallel)    && isBelowLine(lVertical) -> DragDirection.Right
                    isAboveLine(lParallel) && isBelowLine(lVertical) -> DragDirection.RightUp
                    isAboveLine(lParallel) && isOnLine(lVertical)    -> DragDirection.Up
                    isAboveLine(lParallel) && isAboveLine(lVertical) -> DragDirection.LeftUp
                    isOnLine(lParallel)    && isAboveLine(lVertical) -> DragDirection.Left
                    isBelowLine(lParallel) && isAboveLine(lVertical) -> DragDirection.LeftDown
                    isBelowLine(lParallel) && isOnLine(lVertical)    -> DragDirection.Down
                    isBelowLine(lParallel) && isBelowLine(lVertical) -> DragDirection.RightDown
                    else -> DragDirection.Static
                }
            }
        }
        else -> DragDirection.Static
    }

    /**
     * 计算拖动方向到原点的笛卡尔坐标系
     * @return 拖动方向
     */
    fun directionToOInCartesianSystem(): DragDirection {
        return dragDelta.run {
            when {
                x > 0f  && y == 0f -> DragDirection.Right
                x > 0f  && y > 0f  -> DragDirection.RightUp
                x == 0f && y > 0f  -> DragDirection.Up
                x < 0f  && y > 0f  -> DragDirection.LeftUp
                x < 0f  && y == 0f -> DragDirection.Left
                x < 0f  && y < 0f  -> DragDirection.LeftDown
                x == 0f && y < 0f  -> DragDirection.Down
                x > 0f  && y < 0f  -> DragDirection.RightDown
                else -> DragDirection.Static
            }
        }
    }
}
