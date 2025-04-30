package com.tal.xes.bookpage.widget

import com.tal.xes.bookpage.data.AllPoints
import com.tal.xes.bookpage.data.Line
import com.tal.xes.bookpage.data.Line.Companion.getK
import com.tal.xes.bookpage.data.Point
import com.tal.xes.bookpage.func.distanceTo
import kotlin.math.PI

private enum class PageState{
    Loose,
    WMin,
    ThetaMin,
    Tight
}

private const val minWxRatio = 1 / 25f  // W 点在 x轴方向的最小值
private const val stateTightMinWERatio = 1 / 8f //进入tight状态的最小WE长度
private const val minTheta = ((137 * PI) / 180).toFloat() //最小θ，弧度制

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
    // 求出 J - Jf 直线的斜率 の 负分之一
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
    // 求出 J - Jf 直线的斜率 の 负分之一
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

/**
 * ThetaMin 状态下点坐标计算, 书脊翻页到右上角时的状态
 * @param absO 页面左上角坐标点
 * @param absTouchPoint 手指触控点
 * @param absC 页面右下角坐标点
 * @param f HJ 长度
 * @see <a href="https://www.bilibili.com/video/BV1s24y1A7xM/?share_source=copy_web&vd_source=065c25bf82a5ae486ae89a36b38885e3&t=3811">Jetpack Compose 仿真书籍翻页算法原理 PTQBookPageView源码分享</a> — 【精准空降到 1:03:31】
 */
private fun algorithmStateThetaMin(absO: Point, absTouchPoint: Point, absC: Point, f: Float): Pair<Float, AllPoints>{
    val C = absC.toCartesianSystem()
    // 计算页面左下角坐标点
    val A = Point(absO.x, C.y)
    // 计算页面右上角坐标点
    val B = Point(C.x, absO.y)
    val J = absTouchPoint.toCartesianSystem()

    val minWx = minWxRatio * C.x
    val minWE = stateTightMinWERatio * C.x

    // 计算 J 点在页面上的对应点
    val Jf = Point(C.x, C.y + f)
    // 求出 J - Jf 直线的斜率 の 负分之一
    val k = (Jf.x - J.x) / (J.y - Jf.y)
    // 求出松散状态的的 W 点 x 值
    val looseWx = Line.withKAndOnePoint(k, J).getX(C.y)
    // 判断是否是松散状态
    val isLoose = looseWx >= minWx

    // 求出线段 CH
    val lineCH = Line.withKAndOnePoint(getK(minTheta), C)
    val k2 = -1 / lineCH.k

    // 没懂
    val G = Point(C.x, J.y + lineCH.k * (C.x - J.x))

    // 求出 E 点坐标
    val E = Point(Line.withKAndOnePoint(k2, J .. G).getX(C.y), C.y)
    val F = Point(C.x, k2 * (C.x - E.x) + C.y)
    val lineEF = Line.withKAndOnePoint(k2, E)

    val P = lineEF.intersection(lineCH)
    val H = Point(2 * P.x - C.x, 2 * P.y - C.y)

    val isFingerOut = H.distanceTo(J) > f || J.distanceTo(Line.withKAndOnePoint(0f, C)) > f

    val Wx_ = if(isFingerOut){
        J.x - (J.y - C.y) / k2
    }else{
        if(isLoose){
            looseWx
        }else{
            minWx
        }
    }

    val WE = (E.x - Wx_).run { maxOf(minWE, this) }

    val W = Point((E.x - WE).run { maxOf(this, minWx) }, C.y)
    val lineWZ = Line.withKAndOnePoint(k2, W)
    val Z = Point(C.x, lineWZ.getY(C.x))

    val S = W .. E
    val T = F .. Z

    val lineHE = Line.withTwoPoints(E, H)
    val lineHF = Line.withTwoPoints(F, H)
    val lineST = Line.withKAndOnePoint(k2, S)

    val JCopy = lineHF.intersection(lineWZ)
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

    return Pair(E.x - W.x, allPoints)
}

/**
 * Tight 状态下点坐标计算
 * @param absO 页面左上角坐标点
 * @param absC 页面右下角坐标点
 * @param theta 书页翻转角度
 * @see <a href="https://www.bilibili.com/video/BV1s24y1A7xM/?share_source=copy_web&vd_source=065c25bf82a5ae486ae89a36b38885e3&t=4244">Jetpack Compose 仿真书籍翻页算法原理 PTQBookPageView源码分享</a> — 【精准空降到 1:04:04】
 */
private fun algorithmStateTight(absO: Point, absC: Point, theta: Float): Pair<Line, AllPoints>{
    val C = absC.toCartesianSystem()
    // 计算页面左下角坐标点
    val A = Point(absO.x, C.y)
    // 计算页面右上角坐标点
    val B = Point(C.x, absO.y)

    // 已知 Theta 获取 CH 斜率
    val kCH = getK(theta)
    val kEF = -1 / kCH

    val minWE = stateTightMinWERatio * C.x

    val W = Point(minWxRatio * C.x, C.y)
    val Z = Point(C.x, C.y + kEF * (C.x - W.x))

    val E = Point(W.x + minWE, C.y)
    val S = E..W

    val lineCH = Line.withKAndOnePoint(kCH, C)
    val lineEF = Line.withKAndOnePoint(kEF, E)
    val lineST = Line.withKAndOnePoint(kEF, S)
    val lineWZ = Line.withKAndOnePoint(kEF, W)

    val T = Point(C.x, lineST.getY(C.x))
    val F = Point(C.x, lineEF.getY(C.x))

    val P = lineEF.intersection(lineCH)
    val H = Point(2 * P.x - C.x, 2 * P.y - C.y)

    val lineEH = Line.withTwoPoints(E, H)
    val lineFH = Line.withTwoPoints(F, H)

    val I = lineWZ.intersection(lineEH)
    val J = lineWZ.intersection(lineFH)
    val U = lineST.intersection(lineEH)
    val V = lineST.intersection(lineFH)

    val M = U .. S
    val N = V .. T

    val allPoints = AllPoints(
        O = absO,
        A = A,
        B = B,
        C = C,
        H = H,
        I = I,
        J = J,
        M = M,
        N = N,
        S = S,
        T = T,
        U = U,
        V = V,
        W = W,
        Z = Z
    )

    return Pair(lineFH, allPoints)
}
