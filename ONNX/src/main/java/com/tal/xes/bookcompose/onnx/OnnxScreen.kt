package com.tal.xes.bookcompose.onnx

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.tal.xes.bookcompose.onnx.pipeline.StableDiffusionPipeline
import com.tal.xes.bookcompose.onnx.utils.ModelDownloader
import kotlinx.coroutines.launch
import java.io.File

@Composable
fun OnnxScreen(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    
    // Model configuration
    val modelName = "dreamshaper"
    val modelUrl = "https://github.com/ShiftHackZ/Local-Diffusion-Models-SDAI-ONXX/releases/download/patch-25022024/dreamshaper.zip"
    
    // Check if model exists
    var modelBasePath by remember { mutableStateOf<String?>(null) }
    var isCheckingModel by remember { mutableStateOf(true) }
    
    // Download States
    var isDownloading by remember { mutableStateOf(false) }
    var downloadProgress by remember { mutableFloatStateOf(0f) }
    var downloadStatus by remember { mutableStateOf("Not Started") }

    // Generation UI States
    var prompt by remember { mutableStateOf("A beautiful landscape with mountains and lake, highly detailed, 8k") }
    var negativePrompt by remember { mutableStateOf("blurry, low quality, distorted") }
    var steps by remember { mutableFloatStateOf(20f) }
    
    var isGenerating by remember { mutableStateOf(false) }
    var currentStep by remember { mutableIntStateOf(0) }
    var maxSteps by remember { mutableIntStateOf(0) }
    var statusMessage by remember { mutableStateOf("Idle") }
    var resultImage by remember { mutableStateOf<Bitmap?>(null) }
    var isError by remember { mutableStateOf(false) }

    // Initial check for model existence
    LaunchedEffect(Unit) {
        val targetDir = File(File(context.filesDir, "onnx_models"), modelName)
        val unetModel = File(targetDir, "unet/model.ort")
        if (targetDir.exists() && unetModel.exists()) {
            modelBasePath = targetDir.absolutePath
        } else {
            // Clean up invalid/partial state
            if (targetDir.exists()) targetDir.deleteRecursively()
            modelBasePath = null
        }
        isCheckingModel = false
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Top
    ) {
        Text(
            text = "Stable Diffusion ONNX",
            style = MaterialTheme.typography.headlineMedium
        )
        Spacer(modifier = Modifier.height(16.dp))

        if (isCheckingModel) {
            CircularProgressIndicator()
            Text("Checking model installation...")
            return@Column
        }

        // --- DOWNLOAD UI ---
        if (modelBasePath == null) {
            Text(
                text = "Model '$modelName' is required to generate images (~1.1GB).",
                style = MaterialTheme.typography.bodyLarge
            )
            Spacer(modifier = Modifier.height(16.dp))
            
            Button(
                onClick = {
                    isDownloading = true
                    coroutineScope.launch {
                        val path = ModelDownloader.downloadAndExtractModel(
                            context = context,
                            modelUrl = modelUrl,
                            modelName = modelName,
                            onProgress = { progress, status ->
                                downloadProgress = progress
                                downloadStatus = status
                            }
                        )
                        isDownloading = false
                        if (path != null) {
                            modelBasePath = path.absolutePath
                        } else {
                            downloadStatus = "Failed to download / extract."
                        }
                    }
                },
                enabled = !isDownloading,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(if (isDownloading) "Downloading..." else "Download Model")
            }

            Spacer(modifier = Modifier.height(16.dp))
            
            if (isDownloading) {
                LinearProgressIndicator(
                    progress = { downloadProgress },
                    modifier = Modifier.fillMaxWidth()
                )
                Spacer(modifier = Modifier.height(8.dp))
            }
            Text(text = downloadStatus)
            
            return@Column
        }

        // --- GENERATION UI ---
        OutlinedTextField(
            value = prompt,
            onValueChange = { prompt = it },
            label = { Text("Prompt") },
            modifier = Modifier.fillMaxWidth()
        )
        Spacer(modifier = Modifier.height(8.dp))
        
        OutlinedTextField(
            value = negativePrompt,
            onValueChange = { negativePrompt = it },
            label = { Text("Negative Prompt") },
            modifier = Modifier.fillMaxWidth()
        )
        Spacer(modifier = Modifier.height(16.dp))
        
        Text("Inference Steps: ${steps.toInt()}")
        Slider(
            value = steps,
            onValueChange = { steps = it },
            valueRange = 1f..50f,
            steps = 49
        )
        Spacer(modifier = Modifier.height(24.dp))

        Button(
            onClick = {
                if (isGenerating || modelBasePath == null) return@Button
                isGenerating = true
                statusMessage = "Initializing Pipeline..."
                resultImage = null
                isError = false
                currentStep = 0
                maxSteps = steps.toInt()
                
                coroutineScope.launch {
                    try {
                        val pipeline = StableDiffusionPipeline(context, modelBasePath!!)
                        val configSteps = steps.toInt()
                        
                        val bitmap = pipeline.generateImage(
                            prompt = prompt,
                            negativePrompt = negativePrompt,
                            numInferenceSteps = configSteps,
                            onStep = { s, max ->
                                currentStep = s
                                maxSteps = max
                                statusMessage = "Denoising: Step $s / $max"
                            }
                        )
                        
                        if (bitmap != null) {
                            resultImage = bitmap
                            statusMessage = "Generation Complete!"
                        } else {
                            statusMessage = "Failed to generate image (Bitmap null)."
                            isError = true
                        }
                    } catch (e: Exception) {
                        statusMessage = "Error: ${e.message}\nEnsure all models are downloaded correctly."
                        isError = true
                    } finally {
                        isGenerating = false
                    }
                }
            },
            enabled = !isGenerating,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(if (isGenerating) "Generating..." else "Generate Image")
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (isGenerating) {
            LinearProgressIndicator(
                progress = { if (maxSteps > 0) currentStep.toFloat() / maxSteps.toFloat() else 0f },
                modifier = Modifier.fillMaxWidth()
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = statusMessage,
            color = if (isError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface
        )
        
        Spacer(modifier = Modifier.height(16.dp))
        
        resultImage?.let { bmp ->
            Image(
                bitmap = bmp.asImageBitmap(),
                contentDescription = "Generated Image",
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(1f) // 384x384 is 1:1
            )
        }
    }
}
