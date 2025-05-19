package com.tal.xes.vulkan.utils

import android.content.res.AssetManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.withContext

internal fun createNativeHandleFlow(assets: AssetManager, vertexShader: String, fragmentShader: String): Flow<Long> = flow {
    // 在IO上下文中调用JNI方法以避免阻塞主线程
    withContext(Dispatchers.IO) {
        JniUtils.mNativeHandle = JniUtils.createNativeRenderingHandle(assets, vertexShader, fragmentShader)
    }

    emit(JniUtils.mNativeHandle)
}