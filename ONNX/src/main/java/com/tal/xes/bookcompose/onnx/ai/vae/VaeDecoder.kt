package com.tal.xes.bookcompose.onnx.ai.vae

import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtSession
import ai.onnxruntime.OrtSession.SessionOptions
import ai.onnxruntime.providers.NNAPIFlags
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import com.tal.xes.bookcompose.onnx.LocalDiffusionContract
import com.tal.xes.bookcompose.onnx.LocalDiffusionContract.ORT
import com.tal.xes.bookcompose.onnx.LocalDiffusionContract.ORT_KEY_MODEL_FORMAT
import com.tal.xes.bookcompose.onnx.ModelLoader
import com.tal.xes.bookcompose.onnx.OrtEnvironmentManager
import com.tal.xes.bookcompose.onnx.entity.Array3D
import java.util.EnumSet
import kotlin.math.roundToInt

internal class VaeDecoder(
    private val context: Context,
    private val basePath: String,
    private val useNnapi: Boolean = false,
) {

    private var session: OrtSession? = null

    fun decode(input: Map<String?, OnnxTensor?>?): Any {
        initialize()
        val result = session!!.run(input)
        val value = result[0].value
        result.close()
        close()
        return value
    }

    fun convertToImage(
        output: Array3D<FloatArray>,
        width: Int,
        height: Int,
    ): Bitmap {
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565)
        for (y in 0 until height) {
            for (x in 0 until width) {
                val r = (clamp(output[0][0][y][x] / 2 + 0.5) * 255f).roundToInt()
                val g = (clamp(output[0][1][y][x] / 2 + 0.5) * 255f).roundToInt()
                val b = (clamp(output[0][2][y][x] / 2 + 0.5) * 255f).roundToInt()
                val color = Color.rgb(r, g, b)
                bitmap.setPixel(x, y, color)
            }
        }
        return bitmap
    }

    fun close() {
        session?.close()
        session = null
    }

    private fun initialize() {
        if (session != null) return
        val options = SessionOptions()
        // If we want to use hardware acceleration via NNAPI, we can setup it here
        if (useNnapi) {
            options.addNnapi(EnumSet.of(NNAPIFlags.CPU_DISABLED))
        }
        val modelPath = java.io.File(basePath, LocalDiffusionContract.VAE_MODEL).absolutePath
        session = OrtEnvironmentManager.environment.createSession(modelPath, options)
    }

    private fun clamp(value: Double, min: Double = 0.0, max: Double = 1.0): Double = when {
        value < min -> min
        value > max -> max
        else -> value
    }
}
