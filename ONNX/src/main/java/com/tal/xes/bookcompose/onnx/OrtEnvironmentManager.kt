package com.tal.xes.bookcompose.onnx

import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession

object OrtEnvironmentManager {
    val environment: OrtEnvironment by lazy {
        OrtEnvironment.getEnvironment()
    }

    /**
     * Creates a new inference session using the given model bytes.
     */
    fun createSession(modelBytes: ByteArray): OrtSession {
        val options = OrtSession.SessionOptions()
        // Here you can configure thread count, optimizations, or NNAPI flags
        // For example:
        // options.addNnapi() // For hardware acceleration if NNAPI is available
        return environment.createSession(modelBytes, options)
    }
}
