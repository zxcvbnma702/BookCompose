package com.tal.xes.vulkan

import android.graphics.SurfaceTexture
import android.os.Build
import android.util.Log
import android.view.Surface
import android.view.SurfaceView
import android.view.TextureView
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import androidx.navigation.NavController
import com.tal.xes.vulkan.utils.JniUtils
import com.tal.xes.vulkan.utils.createNativeHandleFlow
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun VulkanViewScreen(navController: NavController) {
    val coroutineScope = rememberCoroutineScope()
    Box(modifier = Modifier, contentAlignment = Alignment.Center) {
        // Screen content logic
        AndroidView(factory = { context ->
            TextureView(context).apply {
                surfaceTextureListener = object : TextureView.SurfaceTextureListener {
                    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
                        // Surface is ready, start rendering
                        Log.d("VulkanViewScreen", "Surface is available")
                        coroutineScope.launch  {
                            createNativeHandleFlow(context.assets,
                                "shaders/vert.spv",
                                "shaders/frag.spv").collect { handle ->
                                Log.d("VulkanViewScreen", "Vulkan handle: $handle")
                                JniUtils.transferSurface(Surface(surface))
                            }
                        }
                    }

                    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {}

                    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
                        // Clean up resources if needed
                        return true
                    }

                    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}
                }
            }
        }, update = { view ->
//            val surface = Surface(view.surfaceTexture)
//            JniUtils.updateSurface(surface)
        })
    }

    // 释放协程，避免内存泄漏
    DisposableEffect(Unit) {
        onDispose {
            coroutineScope.cancel()
        }
    }
}