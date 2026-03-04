package com.tal.xes.bookcompose.onnx.utils

import android.content.Context
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.*
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipInputStream

object ModelDownloader {
    
    private const val TAG = "ModelDownloader"

    // Download and extract zip to a specified directory
    suspend fun downloadAndExtractModel(
        context: Context,
        modelUrl: String,
        modelName: String,
        onProgress: (progress: Float, status: String) -> Unit
    ): File? = withContext(Dispatchers.IO) {
        val modelsDir = File(context.filesDir, "onnx_models")
        if (!modelsDir.exists()) {
            modelsDir.mkdirs()
        }
        
        val targetDir = File(modelsDir, modelName)
        val unetModel = File(targetDir, "unet/model.ort")
        if (targetDir.exists() && unetModel.exists()) {
            onProgress(1f, "Model already exists")
            return@withContext targetDir
        } else if (targetDir.exists()) {
            // Clean up partial download
            targetDir.deleteRecursively()
        }
        
        val zipFile = File(modelsDir, "$modelName.zip")
        
        try {
            onProgress(0f, "Downloading $modelName...")
            val url = URL(modelUrl)
            val connection = url.openConnection() as HttpURLConnection
            connection.connect()
            
            if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                throw IOException("Server returned HTTP ${connection.responseCode} ${connection.responseMessage}")
            }
            
            val fileLength = connection.contentLength
            val input = BufferedInputStream(connection.inputStream)
            val output = FileOutputStream(zipFile)
            
            val data = ByteArray(8192)
            var total: Long = 0
            var count: Int
            
            while (input.read(data).also { count = it } != -1) {
                total += count
                if (fileLength > 0) {
                    val progress = (total * 100 / fileLength).toFloat() / 100f
                    onProgress(progress, "Downloading... ${(progress * 100).toInt()}%")
                }
                output.write(data, 0, count)
            }
            
            output.flush()
            output.close()
            input.close()
            
            onProgress(1f, "Extracting...")
            extractZip(zipFile, targetDir)
            
            onProgress(1f, "Complete")
            
            // Delete the zip file after successful extraction
            zipFile.delete()
            
            return@withContext targetDir
        } catch (e: Exception) {
            Log.e(TAG, "Failed to download or extract model", e)
            onProgress(0f, "Error: ${e.message}")
            if (zipFile.exists()) zipFile.delete()
            if (targetDir.exists()) targetDir.deleteRecursively()
            return@withContext null
        }
    }

    private fun extractZip(zipFile: File, targetDir: File) {
        if (!targetDir.exists()) {
            targetDir.mkdirs()
        }
        
        ZipInputStream(BufferedInputStream(FileInputStream(zipFile))).use { zis ->
            var zipEntry = zis.nextEntry
            while (zipEntry != null) {
                val entryName = zipEntry.name ?: ""
                val newFile = File(targetDir, entryName)
                // Prevent Zip Slip vulnerability
                val canonicalDestPath = targetDir.canonicalPath
                val canonicalNewFilePath = newFile.canonicalPath
                if (!canonicalNewFilePath.startsWith(canonicalDestPath + File.separator)) {
                    throw SecurityException("Entry is outside of the target dir: $entryName")
                }
                
                if (zipEntry.isDirectory) {
                    newFile.mkdirs()
                } else {
                    File(newFile.parent).mkdirs()
                    FileOutputStream(newFile).use { fos ->
                        val buffer = ByteArray(8192)
                        var len: Int
                        while (zis.read(buffer).also { len = it } > 0) {
                            fos.write(buffer, 0, len)
                        }
                    }
                }
                zipEntry = zis.nextEntry
            }
            zis.closeEntry()
        }
    }
}
