package com.tal.xes.bookcompose.onnx.ai.scheduler

import com.tal.xes.bookcompose.onnx.entity.LocalDiffusionTensor

internal interface LocalDiffusionScheduler {
    var initNoiseSigma: Double
    fun setTimeSteps(numInferenceSteps: Int): IntArray
    fun scaleModelInput(sample: LocalDiffusionTensor<*>, stepIndex: Int): LocalDiffusionTensor<*>
    fun step(modelOutput: LocalDiffusionTensor<*>, stepIndex: Int, sample: LocalDiffusionTensor<*>): LocalDiffusionTensor<*>
}
