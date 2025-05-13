package com.tal.xes.vulkan

import androidx.compose.foundation.layout.Box
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.navigation.NavController

@Composable
fun VulkanViewScreen(navController: NavController) {
    Box(modifier = Modifier, contentAlignment = Alignment.Center) {
        // Screen content logic
        Text("vulkan")
    }
}