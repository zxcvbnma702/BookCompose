//
// Created by Nio on 2025/5/19.
//
#include <jni.h>
#include <string>

#include "Application.hpp"

#include "VK/VulkanRender.hpp"

extern "C" JNIEXPORT jstring

JNICALL
Java_com_tal_xes_vulkan_utils_JniUtils_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

static size_t Type = 0;

extern "C" {
    JNI_TYPE(void, transferSurface)(JNIEnv *env, jobject thiz,
                              jobject surface, jlong nativeHandle) {
        ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
        if (window == nullptr) {
            LOGE("get window from surface fail!");
            return;
        }

        Application *app = nullptr;
        app = reinterpret_cast<VulkanRender *>(nativeHandle);
        app->run(window);
    }

    JNI_TYPE(jlong, create)(JNIEnv *env, jobject thiz, jobject asset_manager,
            jstring vertex_shader, jstring fragment_shader) {
        // 获取 assetManager
        AAssetManager *assetManager = AAssetManager_fromJava(env, asset_manager);
            if (asset_manager == nullptr) {
            LOGE("get assetManager error");
            return 0;
        }

        const char *vertexShader = env->GetStringUTFChars(vertex_shader, nullptr);
        const char *fragmentShader = env->GetStringUTFChars(fragment_shader, nullptr);

        Application *app = nullptr;
        app = new VulkanRender(assetManager, vertexShader, fragmentShader);

        env->ReleaseStringUTFChars(vertex_shader, vertexShader);
        env->ReleaseStringUTFChars(fragment_shader, fragmentShader);

        return reinterpret_cast<jlong>(app);
    }
}


