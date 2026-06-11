#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kBoltzmann = 1.380649e-23;
constexpr double kAvogadro = 6.02214076e23;
constexpr double kPi = 3.141592653589793238462643383279502884;

struct Case {
  int resolution = 0;
  std::string history;
};

struct Result {
  int resolution = 0;
  double time = 0.0;
  double mass_fraction = 0.0;
  double exact_mass_fraction = 0.0;
  double mass_error_percent = 0.0;
  double volume_fraction = 0.0;
  double exact_volume_fraction = 0.0;
  double volume_error_percent = 0.0;
  double radius = 0.0;
  double exact_radius = 0.0;
  double radius_error_percent = 0.0;
  std::string history;
};

std::vector<std::string> split_csv_line(const std::string &line) {
  std::vector<std::string> fields;
  std::string field;
  std::istringstream input(line);
  while (std::getline(input, field, ',')) {
    fields.push_back(field);
  }
  return fields;
}

double parse_double(const std::string &text, const std::string &name) {
  try {
    std::size_t used = 0;
    const double value = std::stod(text, &used);
    if (used != text.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return value;
  } catch (const std::exception &) {
    throw std::runtime_error("could not parse " + name + " as a number: " + text);
  }
}

int parse_int(const std::string &text, const std::string &name) {
  try {
    std::size_t used = 0;
    const int value = std::stoi(text, &used);
    if (used != text.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return value;
  } catch (const std::exception &) {
    throw std::runtime_error("could not parse " + name + " as an integer: " + text);
  }
}

double percent_error(double actual, double exact) {
  if (std::abs(exact) < std::numeric_limits<double>::epsilon()) {
    return std::abs(actual - exact) < std::numeric_limits<double>::epsilon()
               ? 0.0
               : std::numeric_limits<double>::infinity();
  }
  return 100.0 * std::abs(actual - exact) / std::abs(exact);
}

std::vector<Case> read_cases(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("could not open cases file: " + path);
  }
  std::string line;
  std::getline(input, line);
  std::vector<Case> cases;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    const auto fields = split_csv_line(line);
    if (fields.size() != 2) {
      throw std::runtime_error("expected resolution,history in cases file: " + line);
    }
    cases.push_back({parse_int(fields[0], "resolution"), fields[1]});
  }
  if (cases.size() < 2) {
    throw std::runtime_error("kinetic convergence check needs at least two cases");
  }
  return cases;
}

std::map<std::string, std::string> read_last_history_row(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("could not open history file: " + path);
  }
  std::string header_line;
  if (!std::getline(input, header_line)) {
    throw std::runtime_error("history file is empty: " + path);
  }
  std::string row_line;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty()) {
      row_line = line;
    }
  }
  if (row_line.empty()) {
    throw std::runtime_error("history file has no rows: " + path);
  }
  const auto headers = split_csv_line(header_line);
  const auto fields = split_csv_line(row_line);
  if (headers.size() != fields.size()) {
    throw std::runtime_error("history row/header size mismatch: " + path);
  }
  std::map<std::string, std::string> row;
  for (std::size_t i = 0; i < headers.size(); ++i) {
    row[headers[i]] = fields[i];
  }
  return row;
}

std::string required(const std::map<std::string, std::string> &row,
                     const std::string &key) {
  const auto it = row.find(key);
  if (it == row.end()) {
    throw std::runtime_error("history row is missing column: " + key);
  }
  return it->second;
}

struct Options {
  std::string cases;
  std::string summary;
  double number_density = 7.244e23;
  double temperature = 5000.0;
  double molecular_mass = 5.31352e-26;
  double solid_density = 1800.0;
  double solid_molar_mass = 0.0120107;
  double solid_atoms_per_hit = 1.0;
  double reaction_probability = 1.0;
  double initial_radius = 5.0e-4;
  double max_mass_error_percent = 35.0;
  double max_volume_error_percent = 35.0;
  double max_radius_error_percent = 35.0;
  double min_mass_improvement_percent = 0.0;
};

Options parse_options(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + key);
      }
      return argv[++i];
    };
    if (key == "--cases") {
      options.cases = next();
    } else if (key == "--summary") {
      options.summary = next();
    } else if (key == "--number-density") {
      options.number_density = parse_double(next(), key);
    } else if (key == "--temperature") {
      options.temperature = parse_double(next(), key);
    } else if (key == "--molecular-mass") {
      options.molecular_mass = parse_double(next(), key);
    } else if (key == "--solid-density") {
      options.solid_density = parse_double(next(), key);
    } else if (key == "--solid-molar-mass") {
      options.solid_molar_mass = parse_double(next(), key);
    } else if (key == "--solid-atoms-per-hit") {
      options.solid_atoms_per_hit = parse_double(next(), key);
    } else if (key == "--reaction-probability") {
      options.reaction_probability = parse_double(next(), key);
    } else if (key == "--initial-radius") {
      options.initial_radius = parse_double(next(), key);
    } else if (key == "--max-mass-error-percent") {
      options.max_mass_error_percent = parse_double(next(), key);
    } else if (key == "--max-volume-error-percent") {
      options.max_volume_error_percent = parse_double(next(), key);
    } else if (key == "--max-radius-error-percent") {
      options.max_radius_error_percent = parse_double(next(), key);
    } else if (key == "--min-mass-improvement-percent") {
      options.min_mass_improvement_percent = parse_double(next(), key);
    } else {
      throw std::runtime_error("unknown option: " + key);
    }
  }
  if (options.cases.empty()) {
    throw std::runtime_error("--cases is required");
  }
  if (options.summary.empty()) {
    throw std::runtime_error("--summary is required");
  }
  return options;
}

double kinetic_speed(const Options &options) {
  const double gamma = options.number_density *
                       std::sqrt(kBoltzmann * options.temperature /
                                 (2.0 * kPi * options.molecular_mass));
  const double mass_per_hit =
      options.solid_atoms_per_hit * options.solid_molar_mass / kAvogadro;
  const double flux = gamma * options.reaction_probability * mass_per_hit;
  return flux / options.solid_density;
}

std::vector<Result> evaluate_cases(const Options &options) {
  const double speed = kinetic_speed(options);
  std::vector<Result> results;
  for (const auto &test_case : read_cases(options.cases)) {
    const auto row = read_last_history_row(test_case.history);
    Result result;
    result.resolution = test_case.resolution;
    result.history = test_case.history;
    result.time = parse_double(required(row, "time"), "time");
    result.mass_fraction =
        parse_double(required(row, "mass-fraction"), "mass-fraction");
    result.volume_fraction =
        parse_double(required(row, "volume-fraction"), "volume-fraction");
    result.radius = parse_double(required(row, "radius"), "radius");
    result.exact_radius = std::max(options.initial_radius - speed * result.time, 0.0);
    const double radius_fraction =
        options.initial_radius > 0.0 ? result.exact_radius / options.initial_radius : 0.0;
    result.exact_mass_fraction = radius_fraction * radius_fraction * radius_fraction;
    result.exact_volume_fraction = result.exact_mass_fraction;
    result.mass_error_percent =
        percent_error(result.mass_fraction, result.exact_mass_fraction);
    result.volume_error_percent =
        percent_error(result.volume_fraction, result.exact_volume_fraction);
    result.radius_error_percent = percent_error(result.radius, result.exact_radius);
    results.push_back(result);
  }
  std::sort(results.begin(), results.end(),
            [](const Result &a, const Result &b) { return a.resolution < b.resolution; });
  return results;
}

void write_summary(const std::string &path, const std::vector<Result> &results) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("could not write summary file: " + path);
  }
  output << "resolution,time,mass-fraction,exact-mass-fraction,mass-error-percent,"
            "volume-fraction,exact-volume-fraction,volume-error-percent,"
            "radius,exact-radius,radius-error-percent,history\n";
  output << std::setprecision(17);
  for (const auto &result : results) {
    output << result.resolution << ',' << result.time << ','
           << result.mass_fraction << ',' << result.exact_mass_fraction << ','
           << result.mass_error_percent << ',' << result.volume_fraction << ','
           << result.exact_volume_fraction << ',' << result.volume_error_percent << ','
           << result.radius << ',' << result.exact_radius << ','
           << result.radius_error_percent << ',' << result.history << '\n';
  }
}

int check_results(const Options &options, const std::vector<Result> &results) {
  const auto &coarse = results.front();
  const auto &fine = results.back();
  std::cout << std::setprecision(6);
  for (const auto &result : results) {
    std::cout << "resolution " << result.resolution
              << ": mass error " << result.mass_error_percent
              << "%, volume error " << result.volume_error_percent
              << "%, radius error " << result.radius_error_percent << "%\n";
  }

  bool pass = true;
  if (fine.mass_error_percent > options.max_mass_error_percent) {
    std::cerr << "finest mass error exceeds tolerance: " << fine.mass_error_percent
              << "% > " << options.max_mass_error_percent << "%\n";
    pass = false;
  }
  if (fine.volume_error_percent > options.max_volume_error_percent) {
    std::cerr << "finest volume error exceeds tolerance: " << fine.volume_error_percent
              << "% > " << options.max_volume_error_percent << "%\n";
    pass = false;
  }
  if (fine.radius_error_percent > options.max_radius_error_percent) {
    std::cerr << "finest radius error exceeds tolerance: " << fine.radius_error_percent
              << "% > " << options.max_radius_error_percent << "%\n";
    pass = false;
  }
  const double improvement =
      coarse.mass_error_percent > 0.0
          ? 100.0 * (coarse.mass_error_percent - fine.mass_error_percent) /
                coarse.mass_error_percent
          : 0.0;
  std::cout << "coarse-to-fine mass-error improvement " << improvement << "%\n";
  if (improvement < options.min_mass_improvement_percent) {
    std::cerr << "mass error did not improve enough: " << improvement
              << "% < " << options.min_mass_improvement_percent << "%\n";
    pass = false;
  }
  return pass ? 0 : 1;
}

}  // namespace

int main(int argc, char **argv) {
  try {
    const auto options = parse_options(argc, argv);
    const auto results = evaluate_cases(options);
    write_summary(options.summary, results);
    return check_results(options, results);
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
