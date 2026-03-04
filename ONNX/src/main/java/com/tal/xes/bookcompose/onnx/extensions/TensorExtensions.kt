@file:Suppress("KotlinConstantConditions")

package com.tal.xes.bookcompose.onnx.extensions

import ai.onnxruntime.OnnxTensor
import android.util.Pair
import com.tal.xes.bookcompose.onnx.OrtEnvironmentManager
import com.tal.xes.bookcompose.onnx.entity.Array3D
import com.tal.xes.bookcompose.onnx.entity.LocalDiffusionTensor
import java.nio.FloatBuffer

internal fun duplicate(data: FloatArray, dimensions: LongArray?): LocalDiffusionTensor<*> {
    val env = OrtEnvironmentManager.environment
    val floats = FloatArray(data.size * 2)
    System.arraycopy(data, 0, floats, 0, data.size)
    System.arraycopy(data, 0, floats, data.size, data.size)
    return LocalDiffusionTensor(
        tensor = OnnxTensor.createTensor(
            env,
            FloatBuffer.wrap(floats),
            dimensions
        ),
        buffer = null,
        shape = dimensions,
    )
}

internal fun multipleTensorsByFloat(
    data: FloatArray,
    value: Float,
    dimensions: LongArray?,
): LocalDiffusionTensor<*> {
    val env = OrtEnvironmentManager.environment
    for (i in data.indices) {
        data[i] = data[i] * value
    }
    return LocalDiffusionTensor(
        tensor = OnnxTensor.createTensor(
            env,
            FloatBuffer.wrap(data),
            dimensions
        ),
        buffer = null,
        shape = dimensions,
    )
}

internal fun splitTensor(
    tensorToSplit: Array3D<FloatArray>,
    dimensions: LongArray,
): Pair<Array3D<FloatArray>, Array3D<FloatArray>> {
    val firstTensor = Array(dimensions[0].toInt()) {
        Array(dimensions[1].toInt()) {
            Array(dimensions[2].toInt()) {
                FloatArray(
                    dimensions[3].toInt()
                )
            }
        }
    }
    val secondTensor = Array(dimensions[0].toInt()) {
        Array(dimensions[1].toInt()) {
            Array(dimensions[2].toInt()) {
                FloatArray(
                    dimensions[3].toInt()
                )
            }
        }
    }
    for (i in 0..0) {
        for (j in 0..3) {
            for (k in 0 until dimensions[2]) {
                for (l in 0 until dimensions[3]) {
                    firstTensor[i][j][k.toInt()][l.toInt()] =
                        tensorToSplit[i][j][k.toInt()][l.toInt()]
                    secondTensor[i][j][k.toInt()][l.toInt()] =
                        tensorToSplit[i + 1][j][k.toInt()][l.toInt()]
                }
            }
        }
    }
    return Pair(firstTensor, secondTensor)
}
