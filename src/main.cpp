#include "isthmus_ablation/model.hpp"
#include "isthmus_ablation/parser.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  std::string input_path;
  std::string report_csv_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-in" && i + 1 < argc) {
      input_path = argv[++i];
    } else if (arg == "-report-csv" && i + 1 < argc) {
      report_csv_path = argv[++i];
    } else {
      std::cerr << "usage: ia-core -in <input> [-report-csv <path>]\n";
      return 2;
    }
  }

  if (input_path.empty()) {
    std::cerr << "usage: ia-core -in <input> [-report-csv <path>]\n";
    return 2;
  }

  try {
    auto config = iac::parse_input_file(input_path);
    iac::Model model(std::move(config));
    model.execute(&std::cout);
    if (!report_csv_path.empty()) {
      model.write_verification_csv(report_csv_path);
    }
    model.verify();
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
