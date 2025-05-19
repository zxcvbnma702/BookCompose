package com.tal.xes.vulkan.utils

import android.content.res.AssetManager
import android.view.Surface

object JniUtils {

    enum class Type{
        Vulkan,
        OpenGl
    }

    var mNativeHandle = 0L

    // Used to load the 'vulkan' library on application startup.
    init {
        System.loadLibrary("RenderNative")
    }

    fun createNativeRenderingHandle(assetManager: AssetManager, vertexShader: String, fragmentShader: String): Long{
        mNativeHandle = create(assetManager, vertexShader, fragmentShader)
        return mNativeHandle
    }

    fun transferSurface(surface: Surface){
        transferSurface(surface, mNativeHandle)
    }

    external fun stringFromJNI(): String

    private external fun create(assetManager: AssetManager, vertexShader: String, fragmentShader: String): Long

    private external fun transferSurface(surface: Surface, nativeHandle: Long)
}