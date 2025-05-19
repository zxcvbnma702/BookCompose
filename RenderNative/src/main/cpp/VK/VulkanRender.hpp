//
// Created by Nio on 2025/5/19.
//

#ifndef BOOKCOMPOSE_VULKANRENDER_HPP
#define BOOKCOMPOSE_VULKANRENDER_HPP

#include "../Application.hpp"

class VulkanRender: public Application{

public:
    VulkanRender(AAssetManager *assetManager,
                const char *vertexShader,
                const char *fragmentShader){
        this->assetManager = assetManager;
        this->vertexShader = vertexShader;
        this->fragmentShader = fragmentShader;
    }


    ~VulkanRender() override = default;

    void run(ANativeWindow *window) override;
};


#endif //BOOKCOMPOSE_VULKANRENDER_HPP
