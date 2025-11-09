#ifndef PCH_H_
#define PCH_H_

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include <filesystem>
#include <functional>

#include <future>
#include <mutex>
#include <numeric>

#include <memory>
#include <stack>
#include <deque>
#include <set>
#include <unordered_set>
#include <map>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "utils.h"
#include "config.h"

#endif  // PCH_H_
