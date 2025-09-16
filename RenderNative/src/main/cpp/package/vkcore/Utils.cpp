//
// Created by nio on 2025/7/6.
//
// 通用工具类实现文件
// 提供文件操作、哈希计算、字符串处理等常用功能的具体实现
//

#include "Utils.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace util {

    /**
     * @brief FNV-1a哈希算法实现
     * 
     * FNV-1a (Fowler-Noll-Vo) 算法特点：
     * - 快速：每字节只需要一次乘法和异或操作
     * - 简单：实现代码短小精悍
     * - 分布均匀：对于大多数数据都有良好的哈希分布
     * 
     * 算法步骤：
     * 1. 初始化哈希值为FNV偏移基数 (2166136261)
     * 2. 对每个字节：先异或再乘以FNV质数 (16777619)
     * 3. 返回最终哈希值
     * 
     * @param key 指向要哈希的数据的指针
     * @param len 数据长度（字节数）
     * @return 32位无符号整数哈希值
     */
    uint32_t fnv_hash(const void* key, int len) {
        // 将void*转换为unsigned char*以便按字节处理
        const unsigned char* const p = (unsigned char*)key;
        
        // FNV-1a 32位偏移基数 (offset basis)
        unsigned int h = 2166136261;

        // 遍历每个字节
        for (int i = 0; i < len; i++) {
            // FNV-1a: 先异或当前字节，再乘以FNV质数
            h = (h * 16777619) ^ p[i];
        }

        return h;
    }

    /**
     * @brief 将数据写入文件的实现
     * 
     * 根据isBinary参数选择不同的写入模式：
     * - 二进制模式：使用追加+二进制标志，适合写入图片、音频等
     * - 文本模式：普通文本写入，会覆盖原文件内容
     * 
     * @note 二进制模式使用追加模式，可能不是预期行为
     * @warning 文本模式会将vector<char>转换为string，可能在包含null字符时出现问题
     * @warning 函数不检查文件写入是否成功
     * 
     * @param filePath 目标文件的路径
     * @param fileContents 要写入的数据
     * @param isBinary 是否以二进制模式写入
     */
    void writeFile(const std::string& filePath,
                   const std::vector<char>& fileContents, bool isBinary) {
        if (isBinary) {
            // 二进制模式：以追加+二进制方式打开文件
            std::ofstream out(filePath, std::ios::app | std::ios::binary);
            // 直接写入原始字节数据
            out.write(fileContents.data(), fileContents.size());
            out.close();
        } else {
            // 文本模式：普通文本方式打开（会覆盖原文件）
            std::ofstream out(filePath);
            // 将char数组转换为string后写入
            out << std::string(fileContents.data());
            out.close();
        }
    }

    /**
     * @brief 从文件中读取数据的实现
     * 
     * 实现细节：
     * 1. 使用std::ios::ate标志打开文件，文件指针定位到末尾
     * 2. 通过tellg()获取文件大小
     * 3. 为文本模式预留额外的null终止符空间
     * 4. 重新定位到文件开头并读取所有数据
     * 5. 为文本模式添加null终止符
     * 
     * 错误处理：
     * - 文件无法打开时抛出std::runtime_error异常
     * 
     * @param filePath 要读取的文件路径
     * @param isBinary 是否以二进制模式读取
     * @return 包含文件内容的字符向量
     * @throws std::runtime_error 当文件无法打开时
     */
    std::vector<char> readFile(const std::string& filePath, bool isBinary) {
        // 设置打开模式：ate表示打开时定位到文件末尾
        std::ios_base::openmode mode = std::ios::ate;
        if (isBinary) {
            // 二进制模式：添加binary标志
            mode |= std::ios::binary;
        }
        
        // 打开文件
        std::ifstream file(filePath, mode);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        // 获取文件大小（由于使用了ate，当前位置就是文件大小）
        auto fileSize = (size_t)file.tellg();
        if (!isBinary) {
            // 文本模式：为null终止符预留空间
            fileSize += 1;
        }
        
        // 分配缓冲区
        std::vector<char> buffer(fileSize);
        
        // 重新定位到文件开头
        file.seekg(0);
        
        // 读取文件内容到缓冲区
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();
        
        if (!isBinary) {
            // 文本模式：在末尾添加null终止符
            buffer[buffer.size() - 1] = '\0';
        }
        
        return buffer;
    }

    /**
     * @brief 检查字符串是否以指定后缀结尾的实现
     * 
     * 算法原理：
     * 1. 使用strstr找到part在s中的位置
     * 2. 计算这个位置距离s开头的偏移量
     * 3. 计算s的长度减去part的长度，得到part应该在的位置
     * 4. 比较这两个值是否相等
     * 
     * @warning 如果strstr返回NULL（未找到），会导致未定义行为
     * @warning 不处理空字符串的情况
     * @note 不是最高效的实现方式，更安全的实现应该先检查strstr的返回值和字符串长度
     * 
     * @param s 要检查的主字符串
     * @param part 要查找的后缀字符串
     * @return 如果s以part结尾返回非零值，否则返回0
     */
    int endsWith(const char* s, const char* part) {
        // 计算part在s中的位置，并检查是否在末尾
        // 注意：这里假设strstr能找到part，否则会有未定义行为
        return (strstr(s, part) - s) == (strlen(s) - strlen(part));
    }

    /**
     * @brief 过滤和筛选扩展名集合的实现
     * 
     * 算法步骤：
     * 1. 对两个输入向量分别进行排序
     * 2. 使用std::set_intersection计算交集
     * 3. 将结果插入到unordered_set中返回
     * 
     * @par 时间复杂度:
     * - 排序：O(n log n + m log m)，其中n和m分别是两个向量的大小
     * - 求交集：O(n + m)
     * - 总体：O(n log n + m log m)
     * 
     * @par 使用场景:
     * - Vulkan实例/设备扩展验证
     * - 插件系统中支持功能的匹配
     * - 配置文件中选项的验证
     * 
     * @param availableExtensions 系统可用的扩展名列表
     * @param requestedExtensions 用户请求的扩展名列表
     * @return 两个列表交集的无序集合
     */
    std::unordered_set<std::string> filterExtensions(
            std::vector<std::string> availableExtensions,
            std::vector<std::string> requestedExtensions){
        
        // 对两个向量进行排序，为set_intersection做准备
        std::sort(availableExtensions.begin(), availableExtensions.end());
        std::sort(requestedExtensions.begin(), requestedExtensions.end());
        
        // 创建结果集合
        std::unordered_set<std::string> result;
        
        // 计算两个已排序向量的交集
        // set_intersection要求输入范围已排序
        std::set_intersection(availableExtensions.begin(), availableExtensions.end(),
                              requestedExtensions.begin(), requestedExtensions.end(),
                              std::inserter(result, result.begin()));
        return result;
    };

} // util