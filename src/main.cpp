#include "isthmus_ablation/model.hpp"
#include "isthmus_ablation/parser.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "-in") {
    std::cerr << "usage: ia-core -in <input>\n";
    return 2;
  }

  try {
    auto config = iac::parse_input_file(argv[2]);
    iac::Model model(std::move(config));
    model.execute(&std::cout);
    model.verify();
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
