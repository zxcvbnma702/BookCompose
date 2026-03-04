package com.tal.xes.bookcompose.onnx

import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtSession
import java.nio.FloatBuffer

/**
 * A simplified ONNX inference wrapper based on the Stable Diffusion implementation.
 */
class SimpleInference(private val session: OrtSession) {

    /**
     * Run inference with a single float array input and expect a float array output.
     * This is a generic example representing the ONNX Runtime execution flow.
     */
    fun run(inputData: FloatArray, inputShape: LongArray): FloatArray {
        val env = OrtEnvironmentManager.environment

        // 1. Wrap the input float array into a FloatBuffer
        val buffer = FloatBuffer.wrap(inputData)

        // 2. Identify the input name dynamically (useful for unknown models)
        val inputName = session.inputNames.iterator().next()

        // 3. Create the input tensor
        val inputTensor = OnnxTensor.createTensor(env, buffer, inputShape)

        // 4. Run the model inference
        val result = session.run(mapOf(inputName to inputTensor))

        // 5. Parse the output (assuming it's a 1D float array or can be extracted as such)
        val outputValue = result[0].value
        val outputData = when (outputValue) {
            is Array<*> -> (outputValue[0] as FloatArray) // common for [1, N] outputs
            is FloatArray -> outputValue
            else -> FloatArray(0)
        }

        // 6. Release native resources
        result.close()
        inputTensor.close()

        return outputData
    }
}
