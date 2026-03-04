package com.tal.xes.bookcompose.onnx

import android.content.Context
import java.io.IOException

object ModelLoader {
    /**
     * Load an ONNX or ORT model file from the assets directory as a Byte Array.
     */
    @Throws(IOException::class)
    fun loadModelFromAssets(context: Context, fileName: String): ByteArray {
        return context.assets.open(fileName).use { it.readBytes() }
    }

    /**
     * Load an ONNX or ORT model file from absolute path.
     */
    @Throws(IOException::class)
    fun loadModelFromFile(basePath: String, relativePath: String): ByteArray {
        val file = java.io.File(basePath, relativePath)
        return file.readBytes()
    }
}
