#pragma once

#include "isthmus_ablation/types.hpp"

#include <string>
#include <unordered_map>

namespace iac {

Config parse_input_file(const std::string &path);
Config parse_input_file(const std::string &path,
                        const std::unordered_map<std::string, std::string> &overrides);

} // namespace iac
