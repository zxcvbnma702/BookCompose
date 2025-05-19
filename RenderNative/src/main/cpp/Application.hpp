//
// Created by Nio on 2025/5/19.
//

#ifndef BOOKCOMPOSE_APPLICATION_HPP
#define BOOKCOMPOSE_APPLICATION_HPP

#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>

#include <string>

#include "Log.h"

class Application {
public:
    virtual void run(ANativeWindow *window) = 0;
    virtual ~Application() = default; // 虚析构函数，以便正确释放派生类对象

protected:
    ANativeWindow *window{};
    AAssetManager *assetManager{};
    std::string vertexShader{};
    std::string fragmentShader{};
};

#endif //BOOKCOMPOSE_APPLICATION_HPP
