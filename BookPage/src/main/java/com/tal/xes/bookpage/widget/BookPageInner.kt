package com.tal.xes.bookpage.widget

import com.tal.xes.bookpage.data.AllPoints
import com.tal.xes.bookpage.data.Line
import com.tal.xes.bookpage.data.Point

private enum class PageState{
    Loose,
    WMin,
    ThetaMin,
    Tight
}

private const val minWxRatio = 1 / 25f  // W 点在 x轴方向的最小值

/**
 * 松散状态下点坐标计算
 * @param absO 页面左上角坐标点
 * @param absTouchPoint 手指触控点
 * @param absC 页面右下角坐标点
 * @param f HJ 长度
 */
private fun algorithmStateLoose(absO: Point, absTouchPoint: Point, absC: Point, f: Float): Pair<Float, AllPoints>{
    val C = absC.toCartesianSystem()
    // 计算页面左下角坐标点
    val A = Point(absO.x, C.y)
    // 计算页面右上角坐标点
    val B = Point(C.x, absO.y)
    val J = absTouchPoint.toCartesianSystem()

    // 计算 J 点在页面上的对应点
    val Jf = Point(C.x, C.y + f)
    // 求出 J - Jf 直线的斜率 负分之一
    val k = (Jf.x - J.x) / (J.y - Jf.y)

    // 求出直线 WZ
    val lineWZ = Line.withKAndOnePoint(k, J)
    // 求出 W 点坐标
    val W = Point(lineWZ.getX(C.y), C.y)
    // 求出 Z 点坐标
    val Z = Point(C.x, lineWZ.getY(C.x))

    // 求出直线 EF
    val lineEF = Line.withKAndOnePoint(k, J..Jf)
    // 求出 E 点坐标
    val E = Point(lineEF.getX(C.y), C.y)
    // 求出 F 点坐标
    val F = Point(C.x, lineEF.getY(C.x))

    // 求出 S 点坐标
    val S = W .. E
    // 求出 T 点坐标
    val T = F .. Z

    // 求出直线 CH
    val lineCH = Line.withKAndOnePoint(-1 / k, C)

    // 求出 P 点坐标
    val P = lineEF.intersection(lineCH)
    // 求出 H 点坐标
    val H = Point(2 * P.x - C.x, 2 * P.y - C.y)

    val lineHE = Line.withTwoPoints(E, H)
    val lineHF = Line.withTwoPoints(F, H)
    val lineST = Line.withKAndOnePoint(k, S)

    val JCopy = J.copy()
    val I = lineHE.intersection(lineWZ)
    val U = lineHE.intersection(lineST)
    val V = lineHF.intersection(lineST)

    val M = S .. U
    val N = V .. T

    val allPoints = AllPoints(
        O = absO,
        A = A,
        B = B,
        C = C,
        H = H,
        I = I,
        J = JCopy,
        M = M,
        N = N,
        S = S,
        T = T,
        U = U,
        V = V,
        W = W,
        Z = Z
    )

    return Pair(lineCH.theta(), allPoints)
}

/**
 * WMin 状态下点坐标计算, 左下角翻页到书脊时的状态
 * @param absO 页面左上角坐标点
 * @param absTouchPoint 手指触控点
 * @param absC 页面右下角坐标点
 * @param f HJ 长度
 */
private fun algorithmStateWMin(absO: Point, absTouchPoint:Point, absC: Point, f: Float): Triple<Float, Float, AllPoints>{
    val C = absC.toCartesianSystem()
    // 计算页面左下角坐标点
    val A = Point(absO.x, C.y)
    // 计算页面右上角坐标点
    val B = Point(C.x, absO.y)
    val J = absTouchPoint.toCartesianSystem()
    // 书页到书脊的弯折点
    val W = Point(minWxRatio * C.x, C.y)

    // 计算 J 点在页面上的对应点
    val Jf = Point(C.x, C.y + f)
    // 求出 J - Jf 直线的斜率 负分之一
    val k = (Jf.x - J.x) / (J.y - Jf.y)

    val Z = Point(C.x, k * (C.x - W.x) + C.y)

    // 求出直线 EF
    val lineEF = Line.withKAndOnePoint(k, J..Jf)
    // 求出 E 点坐标
    val E = Point(lineEF.getX(C.y), C.y)
    // 求出 F 点坐标
    val F = Point(C.x, lineEF.getY(C.x))

    // 求出 S 点坐标
    val S = W .. E
    // 求出 T 点坐标
    val T = F .. Z

    // 求出直线 CH
    val lineCH = Line.withKAndOnePoint(-1 / k, C)

    // 求出 P 点坐标
    val P = lineEF.intersection(lineCH)
    // 求出 H 点坐标
    val H = Point(2 * P.x - C.x, 2 * P.y - C.y)

    val lineHE = Line.withTwoPoints(E, H)
    val lineHF = Line.withTwoPoints(F, H)
    val lineST = Line.withKAndOnePoint(k, S)
    val lineWZ = Line.withKAndOnePoint(k, W)

    val I = lineHE.intersection(lineWZ)
    val JCopy = lineHF.intersection(lineWZ)
    val U = lineHE.intersection(lineST)
    val V = lineHF.intersection(lineST)

    val M = S .. U
    val N = V .. T

    val allPoints = AllPoints(
        O = absO,
        A = A,
        B = B,
        C = C,
        H = H,
        I = I,
        J = JCopy,
        M = M,
        N = N,
        S = S,
        T = T,
        U = U,
        V = V,
        W = W,
        Z = Z
    )

    return Triple(lineCH.theta(), E.x - W.x, allPoints)
}

