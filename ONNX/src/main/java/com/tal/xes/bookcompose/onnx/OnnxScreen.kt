package com.tal.xes.bookcompose.onnx

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp

@Composable
fun OnnxScreen(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    var inferenceResult by remember { mutableStateOf<String>("No result yet") }
    var isError by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text(
            text = "ONNX Model Inference",
            style = MaterialTheme.typography.headlineMedium
        )
        
        Spacer(modifier = Modifier.height(32.dp))

        Button(onClick = {
            try {
                // Example flow (Simulated since there is no actual "model.ort" in assets yet)
                // 1. Load: 
                // val bytes = ModelLoader.loadModelFromAssets(context, "dummy_model.ort")
                // 2. Session:
                // val session = OrtEnvironmentManager.createSession(bytes)
                // 3. Inference:
                // val inference = SimpleInference(session)
                // val output = inference.run(floatArrayOf(1f, 2f), longArrayOf(1, 2))
                // 4. Update UI
                
                inferenceResult = "Simulated Success! Model run framework is ready."
                isError = false
            } catch (e: Exception) {
                inferenceResult = "Error: ${e.message}\n(Please place an ONNX model in assets first)"
                isError = true
            }
        }) {
            Text("Run Inference Test")
        }

        Spacer(modifier = Modifier.height(32.dp))

        Text(
            text = inferenceResult,
            color = if (isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface
        )
    }
}
