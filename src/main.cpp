#include "isthmus_ablation/model.hpp"
#include "isthmus_ablation/parser.hpp"

#include <exception>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string sanitize_name(std::string value) {
  for (auto &ch : value) {
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '-';
    if (!ok) {
      ch = '-';
    }
  }
  return value;
}

void write_empty_report_csv(const std::string &path) {
  if (path.empty()) {
    return;
  }
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream out(path);
  out << "quantity,expression,step,time,actual,exact,error,tolerance,tolerance-mode,norm,pass\n";
}

int run_convergence_checks(const std::string &input_path,
                           const iac::Config &base_config,
                           const std::string &report_csv_path) {
  const std::filesystem::path report_dir =
      report_csv_path.empty() ? std::filesystem::path{} :
                                std::filesystem::path(report_csv_path).parent_path();
  std::ofstream convergence_rows;
  std::ofstream order_rows;
  if (!report_csv_path.empty()) {
    std::filesystem::create_directories(report_dir);
    convergence_rows.open(report_dir / "convergence.csv");
    order_rows.open(report_dir / "convergence-order.csv");
    convergence_rows << "quantity,case-index,label,refinement,error,csv\n";
    order_rows << "quantity,first-refinement,last-refinement,first-error,last-error,order,min-order,max-order,monotonic,pass\n";
  }

  for (const auto &check : base_config.convergence_checks) {
    const std::size_t count = check.variables.front().values.size();
    std::vector<double> errors;
    std::vector<double> refinements;
    errors.reserve(count);
    refinements.reserve(count);

    std::cout << "# convergence " << check.check.quantity << "\n";
    for (std::size_t i = 0; i < count; ++i) {
      std::unordered_map<std::string, std::string> overrides;
      for (const auto &variable : check.variables) {
        overrides[variable.name] = variable.values[i];
      }
      auto config = iac::parse_input_file(input_path, overrides);
      config.convergence_checks.clear();
      config.checks.clear();
      config.checks.push_back(check.check);
      iac::Model model(std::move(config));
      model.execute(nullptr);
      const double error = model.verification_error(check.check);
      errors.push_back(error);
      refinements.push_back(std::stod(check.variables.front().values[i]));

      std::cout << "#   case";
      std::string label;
      for (const auto &variable : check.variables) {
        const auto &value = overrides.at(variable.name);
        std::cout << ' ' << variable.name << '=' << value;
        if (!label.empty()) {
          label += ", ";
        }
        label += variable.name + "=" + value;
      }
      std::cout << " error=" << error << '\n';

      if (convergence_rows) {
        const std::string stem = "convergence-" + sanitize_name(check.check.quantity) +
                                 "-" + std::to_string(i + 1) + ".csv";
        const auto case_csv = report_dir / stem;
        model.write_verification_csv(case_csv.string());
        convergence_rows << check.check.quantity << ',' << (i + 1) << ','
                         << '"' << label << '"' << ',' << refinements.back() << ','
                         << std::setprecision(17) << error << ',' << stem << '\n';
      }
    }

    if (check.require_monotonic) {
      for (std::size_t i = 1; i < errors.size(); ++i) {
        if (!(errors[i - 1] > errors[i])) {
          std::cerr << "error: convergence failed for '" << check.check.quantity
                    << "': error is not monotonically decreasing\n";
          return 1;
        }
      }
    }
    const double first_resolution = refinements.front();
    const double last_resolution = refinements.back();
    const double order = std::log(errors.front() / errors.back()) /
                         std::log(last_resolution / first_resolution);
    std::cout << "#   apparent-order=" << order << '\n';
    const bool order_pass = order >= check.min_order && order <= check.max_order;
    if (order_rows) {
      order_rows << check.check.quantity << ',' << first_resolution << ','
                 << last_resolution << ',' << std::setprecision(17) << errors.front()
                 << ',' << errors.back() << ',' << order << ',' << check.min_order
                 << ',' << check.max_order << ','
                 << (check.require_monotonic ? "yes" : "no") << ','
                 << (order_pass ? "yes" : "no") << '\n';
    }
    if (!order_pass) {
      std::cerr << "error: convergence failed for '" << check.check.quantity
                << "': apparent order " << order << " outside ["
                << check.min_order << ", " << check.max_order << "]\n";
      return 1;
    }
  }
  return 0;
}

} // namespace

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
    const bool run_base_case =
        !config.checks.empty() || config.convergence_checks.empty();
    if (run_base_case) {
      iac::Model model(config);
      model.execute(&std::cout);
      if (!report_csv_path.empty()) {
        model.write_verification_csv(report_csv_path);
      }
      model.verify();
    } else if (!report_csv_path.empty()) {
      write_empty_report_csv(report_csv_path);
    }
    const int convergence_status = run_convergence_checks(input_path, config, report_csv_path);
    if (convergence_status != 0) {
      return convergence_status;
    }
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
