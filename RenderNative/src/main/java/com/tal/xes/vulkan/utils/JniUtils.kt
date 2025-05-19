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

    external fun stringFromJNI(): String

    external fun create(assetManager: AssetManager, vertexShader: String, fragmentShader: String): Long

    external fun transferSurface(surface: Surface, nativeHandle: Long)
}