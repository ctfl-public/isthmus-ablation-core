#include "isthmus_ablation/expression.hpp"

#include "isthmus_ablation/types.hpp"

#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>

namespace iac {
namespace {

class ExpressionParser {
public:
  ExpressionParser(const std::string &expression,
                   const std::unordered_map<std::string, double> &variables)
      : expression_(expression), variables_(variables) {}

  double parse() {
    const double value = parse_expression();
    skip_spaces();
    if (position_ != expression_.size()) {
      throw RuntimeError("unexpected token in expression '" + expression_ + "'");
    }
    return value;
  }

private:
  const std::string &expression_;
  const std::unordered_map<std::string, double> &variables_;
  std::size_t position_ = 0;

  void skip_spaces() {
    while (position_ < expression_.size() &&
           std::isspace(static_cast<unsigned char>(expression_[position_]))) {
      ++position_;
    }
  }

  bool consume(char token) {
    skip_spaces();
    if (position_ < expression_.size() && expression_[position_] == token) {
      ++position_;
      return true;
    }
    return false;
  }

  double parse_expression() {
    double value = parse_term();
    while (true) {
      if (consume('+')) {
        value += parse_term();
      } else if (consume('-')) {
        value -= parse_term();
      } else {
        return value;
      }
    }
  }

  double parse_term() {
    double value = parse_power();
    while (true) {
      if (consume('*')) {
        value *= parse_power();
      } else if (consume('/')) {
        value /= parse_power();
      } else {
        return value;
      }
    }
  }

  double parse_power() {
    double value = parse_unary();
    if (consume('^')) {
      value = std::pow(value, parse_power());
    }
    return value;
  }

  double parse_unary() {
    if (consume('+')) {
      return parse_unary();
    }
    if (consume('-')) {
      return -parse_unary();
    }
    return parse_primary();
  }

  double parse_primary() {
    skip_spaces();
    if (consume('(')) {
      const double value = parse_expression();
      if (!consume(')')) {
        throw RuntimeError("missing ')' in expression '" + expression_ + "'");
      }
      return value;
    }
    if (position_ < expression_.size() &&
        (std::isdigit(static_cast<unsigned char>(expression_[position_])) ||
         expression_[position_] == '.')) {
      return parse_number();
    }
    return parse_variable();
  }

  double parse_number() {
    const std::size_t start = position_;
    while (position_ < expression_.size()) {
      const char c = expression_[position_];
      if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' && c != 'e' &&
          c != 'E' && c != '+' && c != '-') {
        break;
      }
      if ((c == '+' || c == '-') && position_ != start &&
          expression_[position_ - 1] != 'e' && expression_[position_ - 1] != 'E') {
        break;
      }
      ++position_;
    }
    try {
      return std::stod(expression_.substr(start, position_ - start));
    } catch (const std::exception &) {
      throw RuntimeError("invalid number in expression '" + expression_ + "'");
    }
  }

  double parse_variable() {
    skip_spaces();
    const std::size_t start = position_;
    while (position_ < expression_.size()) {
      const char c = expression_[position_];
      if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' &&
          c != ':') {
        break;
      }
      ++position_;
    }
    if (start == position_) {
      throw RuntimeError("expected expression term in '" + expression_ + "'");
    }
    const std::string name = expression_.substr(start, position_ - start);
    const auto found = variables_.find(name);
    if (found == variables_.end()) {
      throw RuntimeError("unknown expression variable '" + name + "'");
    }
    return found->second;
  }
};

} // namespace

double evaluate_expression(const std::string &expression,
                           const std::unordered_map<std::string, double> &variables) {
  return ExpressionParser(expression, variables).parse();
}

} // namespace iac
