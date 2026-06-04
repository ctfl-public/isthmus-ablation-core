#pragma once

#include <string>
#include <unordered_map>

namespace iac {

double evaluate_expression(const std::string &expression,
                           const std::unordered_map<std::string, double> &variables);

} // namespace iac

