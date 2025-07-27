//
// Created by nio on 2025/7/7.
//

#ifndef BOOKCOMPOSE_PHYSICALDEVICE_H
#define BOOKCOMPOSE_PHYSICALDEVICE_H

#include <list>
#include <optional>
#include <string>
#include <set>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore{
    class PhysicalDevice final{
    public:
        explicit PhysicalDevice()= default;
        explicit PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                                const std::vector<std::string>& requestedExtensions,
                                bool printEnumerations = false, bool enableRayTracing = false);

        [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;

        [[nodiscard]] const std::vector<std::string>& extensions() const;

        /**
         * @brief Vulkan 队列族功能分布图
         *
         * <table>
         *   <tr>
         *     <th>Queue Family Index</th>
         *     <th>Graphics</th>
         *     <th>Compute</th>
         *     <th>Transfer</th>
         *     <th>Sparse Binding</th>
         *     <th>Presentation</th>
         *   </tr>
         *   <tr>
         *     <td>0</td>
         *     <td>✔</td>
         *     <td>✔</td>
         *     <td>✔</td>
         *     <td>✘</td>
         *     <td>✘</td>
         *   </tr>
         *   <tr>
         *     <td>1</td>
         *     <td>✘</td>
         *     <td>✔</td>
         *     <td>✔</td>
         *     <td>✘</td>
         *     <td>✘</td>
         *   </tr>
         *   <tr>
         *     <td>2</td>
         *     <td>✘</td>
         *     <td>✘</td>
         *     <td>✔</td>
         *     <td>✔</td>
         *     <td>✘</td>
         *   </tr>
         *   <tr>
         *     <td>3</td>
         *     <td>✔</td>
         *     <td>✘</td>
         *     <td>✘</td>
         *     <td>✘</td>
         *     <td>✔ (Present)</td>
         *   </tr>
         * </table>
         *
         * <p><b>说明：</b></p>
         * <ul>
         *   <li>一个物理设备通常包含多个 Queue Family，每个支持不同类型的操作。</li>
         *   <li><b>Graphics：</b> 图形绘制命令（如 vkCmdDraw）</li>
         *   <li><b>Compute：</b> 计算着色器（如 vkCmdDispatch）</li>
         *   <li><b>Transfer：</b> 内存复制与传输命令（如 vkCmdCopyBuffer）</li>
         *   <li><b>Sparse Binding：</b> 支持稀疏资源（如稀疏纹理）</li>
         *   <li><b>Presentation：</b> 是否支持图像呈现到屏幕（需配合 Surface 检查）</li>
         * </ul>
         *
         * <p><b>队列选择逻辑：</b></p>
         * <ol>
         *   <li>使用 vkGetPhysicalDeviceQueueFamilyProperties 查询 queueFlags。</li>
         *   <li>使用 vkGetPhysicalDeviceSurfaceSupportKHR 判断是否支持 Present。</li>
         *   <li>优先选择支持单一功能（如 Transfer-only）的队列以优化并行执行。</li>
         * </ol>
         */
        void reserveQueues(VkQueueFlags requestedQueueTypes, VkSurfaceKHR surface);

        /**
         * @brief 返回当前物理设备中所选队列族的索引与数量对。
         *
         * <p>该函数收集了所有当前已选择的支持功能的队列族，包括：</p>
         * <ul>
         *   <li><b>Graphics（图形）</b></li>
         *   <li><b>Compute（计算）</b></li>
         *   <li><b>Transfer（传输）</b></li>
         *   <li><b>Sparse Binding（稀疏资源绑定）</b></li>
         *   <li><b>Presentation（图像呈现）</b></li>
         * </ul>
         *
         * <p>每个队列族表示为一个 <code>std::pair&lt;uint32_t, uint32_t&gt;</code>：</p>
         * <ul>
         *   <li><code>first</code> 为队列族索引（queue family index）</li>
         *   <li><code>second</code> 为该队列族支持的队列数量（queue count）</li>
         * </ul>
         *
         * <p>为避免重复队列族索引，使用 <code>std::set</code> 去重。</p>
         *
         * @return <code>std::vector&lt;std::pair&lt;uint32_t, uint32_t&gt;&gt;</code> 包含各功能所使用的唯一队列族索引及其队列数量。
         *
         * @note 如果多个功能（如图形和呈现）共享同一队列族，只会记录一次。
         *
         * @see reserveQueues
         * @see graphicsFamilyIndex
         */
        [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> queueFamilyIndexAndCount() const;


        [[nodiscard]] std::optional<uint32_t> graphicsFamilyIndex() const;
        [[nodiscard]] std::optional<uint32_t> computeFamilyIndex() const;
        [[nodiscard]] std::optional<uint32_t> transferFamilyIndex() const;
        [[nodiscard]] std::optional<uint32_t> sparseFamilyIndex() const;
        [[nodiscard]] std::optional<uint32_t> presentationFamilyIndex() const;

        [[nodiscard]] uint32_t graphicsFamilyCount() const { return graphicsQueueCount_; }
        [[nodiscard]] uint32_t computeFamilyCount() const { return computeQueueCount_; }
        [[nodiscard]] uint32_t transferFamilyCount() const { return transferQueueCount_; }
        [[nodiscard]] uint32_t sparseFamilyCount() const { return sparseQueueCount_; }
        [[nodiscard]] uint32_t presentationFamilyCount() const {
            return presentationQueueCount_;
        }

        [[nodiscard]] const std::unordered_set<std::string>& enabledExtensions() const {
            return enabledExtensions_;
        }
    private:
//        VkPhysicalDeviceProperties2 properties_ = {
//                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
//                .pNext = &rayTracingPipelineProperties_,
//        };

    private:
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        // 设备支持的扩展
        std::vector<std::string> extensions_;

        // 图形队列族的索引。用于处理图形渲染任务（如绘制命令）
        std::optional<uint32_t> graphicsFamilyIndex_;
        uint32_t graphicsQueueCount_ = 0;
        // 计算队列族的索引。用于纯计算任务，例如使用 vkCmdDispatch()
        std::optional<uint32_t> computeFamilyIndex_;
        uint32_t computeQueueCount_ = 0;
        // 数据传输专用队列族的索引。用于加速 vkCmdCopyBuffer() 等操作
        std::optional<uint32_t> transferFamilyIndex_;
        uint32_t transferQueueCount_ = 0;
        // 稀疏绑定队列族索引。用于稀疏资源（如稀疏纹理、虚拟内存）
        std::optional<uint32_t> sparseFamilyIndex_;
        uint32_t sparseQueueCount_ = 0;
        // 支持显示（呈现）图像到窗口系统的队列族索引
        std::optional<uint32_t> presentationFamilyIndex_;
        uint32_t presentationQueueCount_ = 0;

        // 队列族属性
        std::vector<VkQueueFamilyProperties> queueFamilyProperties_;
        // 要使用的扩展
        std::unordered_set<std::string> enabledExtensions_;
    };
}


#endif //BOOKCOMPOSE_PHYSICALDEVICE_H
