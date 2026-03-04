package com.tal.xes.bookcompose.onnx.pipeline

import ai.onnxruntime.OnnxTensor
import android.content.Context
import android.graphics.Bitmap
import android.util.Log
import com.tal.xes.bookcompose.onnx.OrtEnvironmentManager
import com.tal.xes.bookcompose.onnx.ai.tokenizer.EnglishTextTokenizer
import com.tal.xes.bookcompose.onnx.ai.unet.UNet
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class StableDiffusionPipeline(
    private val context: Context, 
    private val basePath: String,
    private val useNnapi: Boolean = false
) {
    
    private val tokenizer = EnglishTextTokenizer(context, basePath)
    private val uNet = UNet(context, basePath, useNnapi)
    
    private val TAG = "StableDiffusionPipeline"

    suspend fun generateImage(
        prompt: String,
        negativePrompt: String = "",
        seedNum: Long = 0L,
        numInferenceSteps: Int = 20,
        guidanceScale: Double = 7.5,
        width: Int = 384,
        height: Int = 384,
        onStep: (step: Int, maxStep: Int) -> Unit = { _, _ -> }
    ): Bitmap? = withContext(Dispatchers.Default) {
        var resultBitmap: Bitmap? = null
        try {
            uNet.setCallback(object : UNet.Callback {
                override fun onStep(maxStep: Int, step: Int) {
                    onStep(step, maxStep)
                }

                override fun onBuildImage(status: Int, bitmap: Bitmap?) {
                    resultBitmap = bitmap
                }
            })

            Log.d(TAG, "Initializing Tokenizer...")
            tokenizer.initialize()
            
            val textTokenized = tokenizer.encode(prompt)
            val negTokenized = tokenizer.createUnconditionalInput(negativePrompt)

            val textPromptEmbeddings = tokenizer.tensor(textTokenized)
            val unConditionalEmbedding = tokenizer.tensor(negTokenized)
            val textEmbeddingArray = Array(2) {
                Array(tokenizer.maxLength) {
                    FloatArray(768)
                }
            }

            val textPromptEmbeddingArray = textPromptEmbeddings!!.floatBuffer.array()
            val unConditionalEmbeddingArray = unConditionalEmbedding!!.floatBuffer.array()
            for (i in textPromptEmbeddingArray.indices) {
                textEmbeddingArray[0][i / 768][i % 768] = unConditionalEmbeddingArray[i]
                textEmbeddingArray[1][i / 768][i % 768] = textPromptEmbeddingArray[i]
            }

            val textEmbeddings = OnnxTensor.createTensor(OrtEnvironmentManager.environment, textEmbeddingArray)
            tokenizer.close()

            Log.d(TAG, "Initializing UNet & VAE...")
            uNet.initialize()
            Log.d(TAG, "Starting UNet Inference Loop...")
            uNet.inference(
                seedNum = seedNum,
                numInferenceSteps = numInferenceSteps,
                textEmbeddings = textEmbeddings,
                guidanceScale = guidanceScale,
                batchSize = 1,
                width = width,
                height = height,
            )
        } catch (e: Exception) {
            Log.e(TAG, "Caught exception during image generation", e)
        } finally {
            Log.d(TAG, "Cleaning up resources...")
            tokenizer.close()
            uNet.close()
        }
        return@withContext resultBitmap
    }
}
