//
// Created by nio on 2025/7/6.
//

#include "Utils.h"

namespace util {

    /**
     * @brief 过滤可用扩展名
     * @param availableExtensions
     * @param requestedExtensions
     * @return
     */
    std::unordered_set<std::string> filterExtensions(
            std::vector<std::string> availableExtensions,
            std::vector<std::string> requestedExtensions){
        std::sort(availableExtensions.begin(), availableExtensions.end());
        std::sort(requestedExtensions.begin(), requestedExtensions.end());
        std::unordered_set<std::string> result;
        std::set_intersection(availableExtensions.begin(), availableExtensions.end(),
                              requestedExtensions.begin(), requestedExtensions.end(),
                              std::inserter(result, result.begin()));
        return result;
    };

} // util