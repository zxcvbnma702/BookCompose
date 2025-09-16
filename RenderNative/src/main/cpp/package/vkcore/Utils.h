//
// Created by nio on 2025/7/6.
// 
// 通用工具类头文件，包含常用的工具函数和宏定义
// 主要功能：文件操作、哈希计算、字符串处理、扩展名过滤等
//

#ifndef BOOKCOMPOSE_UTILS_H
#define BOOKCOMPOSE_UTILS_H

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief 断言宏，用于调试时检查条件
 * @param expr 要检查的表达式
 * @param message 断言失败时的消息（在Release版本中被忽略）
 * 
 * @code
 * ASSERT(ptr != nullptr, "Pointer should not be null");
 * @endcode
 * 
 * @note 在Release版本中，message参数会被void(message)处理以避免未使用警告
 */
#define ASSERT(expr, message) \
    {                         \
        void(message);        \
        assert(expr);         \
    }

/**
 * @brief 只允许移动构造和移动赋值的宏
 * @param CLASS_NAME 类名
 * 
 * 该宏会：
 * - 删除拷贝构造函数和拷贝赋值运算符
 * - 保留默认的移动构造函数和移动赋值运算符
 * 
 * @code
 * class MyClass {
 * public:
 *     MOVABLE_ONLY(MyClass)
 *     // 其他成员...
 * };
 * @endcode
 * 
 * @note 适用场景：管理独占资源的类，如RAII包装器
 */
#define MOVABLE_ONLY(CLASS_NAME) \
    CLASS_NAME(const CLASS_NAME &) = delete; \
    CLASS_NAME &operator=(const CLASS_NAME &) = delete; \
    CLASS_NAME(CLASS_NAME &&) noexcept = default; \
    CLASS_NAME &operator=(CLASS_NAME &&) noexcept = default; \

namespace util {

    /**
     * @brief 使用FNV-1a算法计算哈希值
     * @param key 要计算哈希的数据指针
     * @param len 数据长度（字节数）
     * @return 32位哈希值
     * 
     * FNV-1a是一种快速、简单的非加密哈希算法，适用于：
     * - 哈希表的键值计算
     * - 快速数据校验
     * - 字符串哈希
     * 
     * @code
     * std::string str = "hello";
     * uint32_t hash = fnv_hash(str.c_str(), str.length());
     * @endcode
     */
    uint32_t fnv_hash(const void* key, int len);

    /**
     * @brief 从文件中读取数据
     * @param filePath 文件路径
     * @param isBinary 是否以二进制模式读取，默认为false（文本模式）
     * @return 文件内容的字节数组
     * @throws std::runtime_error 当文件无法打开时抛出异常
     * 
     * 功能说明：
     * - 文本模式：会在末尾添加null终止符('\0')
     * - 二进制模式：原样读取文件内容
     * - 自动获取文件大小并预分配内存
     * 
     * @code
     * // 读取文本文件
     * auto textData = readFile("config.txt", false);
     * std::string content(textData.data());
     * 
     * // 读取二进制文件
     * auto binaryData = readFile("image.png", true);
     * @endcode
     */
    std::vector<char> readFile(const std::string& filePath, bool isBinary = false);

    /**
     * @brief 将数据写入文件
     * @param filePath 目标文件路径
     * @param fileContents 要写入的数据
     * @param isBinary 是否以二进制模式写入，默认为false（文本模式）
     * 
     * 功能说明：
     * - 文本模式：以文本格式写入，会进行字符编码转换
     * - 二进制模式：原样写入字节数据，使用追加模式
     * - 如果文件不存在会自动创建
     * - 如果文件存在会覆盖内容（文本模式）或追加内容（二进制模式）
     * 
     * @code
     * // 写入文本
     * std::string text = "Hello World";
     * std::vector<char> textData(text.begin(), text.end());
     * writeFile("output.txt", textData, false);
     * 
     * // 写入二进制数据
     * writeFile("output.bin", binaryData, true);
     * @endcode
     */
    void writeFile(const std::string& filePath,
                   const std::vector<char>& fileContents, bool isBinary = false);

    /**
     * @brief 检查字符串是否以指定后缀结尾
     * @param s 要检查的字符串
     * @param part 后缀字符串
     * @return 如果以part结尾返回非零值，否则返回0
     * 
     * 实现原理：
     * - 使用strstr查找part在s中的位置
     * - 检查找到的位置是否正好在s的末尾
     * 
     * @code
     * if (endsWith("hello.txt", ".txt")) {
     *     // 文件是文本文件
     * }
     * @endcode
     * 
     * @warning 该函数对空字符串的行为未定义，使用前应检查参数有效性
     */
    int endsWith(const char* s, const char* part);

    /**
     * @brief 过滤和筛选扩展名集合
     * @param availableExtensions 可用的扩展名列表
     * @param requestedExtensions 请求的扩展名列表
     * @return 两个列表的交集
     * 
     * 功能说明：
     * - 计算两个扩展名列表的交集
     * - 内部会对输入列表进行排序以提高效率
     * - 返回的是无序集合，保证元素唯一性
     * 
     * 使用场景：
     * - Vulkan扩展筛选
     * - 插件系统中的功能匹配
     * - 配置验证
     * 
     * @code
     * std::vector<std::string> available = {"ext1", "ext2", "ext3"};
     * std::vector<std::string> requested = {"ext2", "ext4"};
     * auto supported = filterExtensions(available, requested);
     * // supported 包含 {"ext2"}
     * @endcode
     */
    std::unordered_set<std::string> filterExtensions(
            std::vector<std::string> availableExtensions,
            std::vector<std::string> requestedExtensions);

    /**
     * @brief 组合多个值的哈希值（模板函数）
     * @tparam T 第一个值的类型
     * @tparam Rest 其余值的类型包
     * @param seed 哈希种子值，会被修改以包含新的哈希值
     * @param v 第一个要哈希的值
     * @param rest 其余要哈希的值
     * 
     * 功能说明：
     * - 使用标准的哈希组合算法
     * - 递归处理多个参数
     * - 魔数0x9e3779b9来自黄金比例的32位近似值
     * - 通过位移和异或操作混合哈希值
     * 
     * @code
     * std::size_t seed = 0;
     * hash_combine(seed, 42, "hello", 3.14f);
     * // seed现在包含了所有值的组合哈希
     * @endcode
     * 
     * 典型应用：
     * - 为复合对象计算哈希值
     * - 实现自定义类型的std::hash特化
     * - 创建多键哈希表
     */
    template <typename T, typename... Rest>
    void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
        seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        (hash_combine(seed, rest), ...);
    }

} // util

#endif //BOOKCOMPOSE_UTILS_H
