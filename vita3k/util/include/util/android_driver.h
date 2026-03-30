#pragma once

#include <string>

namespace android_driver {

void *load_custom_vulkan_driver(const std::string &driver_name);

}
