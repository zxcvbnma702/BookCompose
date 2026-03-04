package com.tal.xes.bookcompose

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.tal.xes.vulkan.VulkanViewScreen
import com.tal.xes.bookcompose.onnx.OnnxScreen

@Composable
fun MainScreen(modifier: Modifier){
    val navController = rememberNavController()

    // Set up the navigation graph
    NavHost(
        navController = navController,
        startDestination = "home",
        modifier = modifier
    ) {
        composable("home") { HomeScreen(navController) }

        // Vulkan 页面的路由
        composable("VK") {
            VulkanViewScreen(navController)
        }

        // ONNX 推理页面路由
        composable("ONNX") {
            OnnxScreen()
        }
    }
}