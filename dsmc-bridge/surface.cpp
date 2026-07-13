#include "surface.h"

#include "error.h"
#include "iacbridge.h"
#include "comm.h"
#include "fix.h"
#include "input.h"
#include "modify.h"
#include "run.h"
#include "surf.h"
#include "update.h"
#include "variable.h"

#include <mpi.h>

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace SPARTA_NS;

namespace {

constexpr double AVOGADRO = 6.02214076e23;

struct ReactionFileInfo {
  double probability = 1.0;
  double solid_mass = 0.0;
};

struct NormalSelector {
  bool enabled = false;
  double direction[3] = {0.0, 0.0, 0.0};
  double min_cos = 0.0;
};

struct ConvergenceWindow {
  std::vector<double> values;
  int max_size = 1;

  void push(double value) {
    values.push_back(value);
    if (static_cast<int>(values.size()) > max_size) {
      values.erase(values.begin());
    }
  }

  bool full() const { return static_cast<int>(values.size()) >= max_size; }

  double mean() const {
    if (values.empty()) {
      return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) /
           static_cast<double>(values.size());
  }

  double cv() const {
    if (values.empty()) {
      return 0.0;
    }
    const double avg = mean();
    double variance = 0.0;
    for (const double value : values) {
      const double diff = value - avg;
      variance += diff * diff;
    }
    variance /= static_cast<double>(values.size());
    const double scale = std::sqrt(avg * avg + 1.0e-300);
    return std::sqrt(variance) / scale;
  }
};

struct SurfaceValueStats {
  std::size_t selected_count = 0;
  std::size_t positive_count = 0;
  double selected_area = 0.0;
  double positive_area = 0.0;
  double positive_value_sum = 0.0;
  double positive_area_value_sum = 0.0;
  double max_positive_value = 0.0;
};

const char *value_after(int narg, char **arg, const char *key) {
  for (int i = 0; i + 1 < narg; ++i) {
    if (std::strcmp(arg[i], key) == 0) {
      return arg[i + 1];
    }
  }
  return nullptr;
}

bool optional_bool_after(int narg, char **arg, const char *key, bool default_value,
                         Error *error, const char *command) {
  const char *value = value_after(narg, arg, key);
  if (!value) {
    return default_value;
  }
  if (std::strcmp(value, "yes") == 0 || std::strcmp(value, "true") == 0 ||
      std::strcmp(value, "1") == 0) {
    return true;
  }
  if (std::strcmp(value, "no") == 0 || std::strcmp(value, "false") == 0 ||
      std::strcmp(value, "0") == 0) {
    return false;
  }
  error->all(FLERR, (std::string(command) + " " + key +
                     " must be yes or no").c_str());
  return default_value;
}

int direct_mass_flux_column(int narg, char **arg, Error *error, const char *command) {
  const char *column_value = value_after(narg, arg, "column");
  const char *quantity_value = value_after(narg, arg, "quantity");
  if (column_value && quantity_value) {
    error->all(FLERR, (std::string(command) + " cannot use both column and quantity").c_str());
  }
  if (quantity_value) {
    if (std::strcmp(quantity_value, "mass-flux") == 0) {
      return 1;
    }
    error->all(FLERR, (std::string(command) + " quantity must be mass-flux").c_str());
  }
  if (column_value) {
    return std::atoi(column_value);
  }
  error->all(FLERR, (std::string(command) + " requires quantity mass-flux or column <N>").c_str());
  return 0;
}

NormalSelector parse_optional_normal_selector(int narg, char **arg, Error *error,
                                              const char *command) {
  NormalSelector selector;
  const char *select = value_after(narg, arg, "select");
  if (!select) {
    return selector;
  }
  if (std::strcmp(select, "all") == 0) {
    return selector;
  }
  if (std::strcmp(select, "normal") != 0) {
    error->all(FLERR, (std::string(command) + " select must be all or normal").c_str());
  }
  const char *nx = value_after(narg, arg, "nx");
  const char *ny = value_after(narg, arg, "ny");
  const char *nz = value_after(narg, arg, "nz");
  if (!nx || !ny || !nz) {
    error->all(FLERR, (std::string(command) + " select normal requires nx, ny, and nz").c_str());
  }
  selector.direction[0] = std::atof(nx);
  selector.direction[1] = std::atof(ny);
  selector.direction[2] = std::atof(nz);
  const double norm = std::sqrt(selector.direction[0] * selector.direction[0] +
                                selector.direction[1] * selector.direction[1] +
                                selector.direction[2] * selector.direction[2]);
  if (norm <= 0.0) {
    error->all(FLERR, (std::string(command) + " select normal direction cannot be zero").c_str());
  }
  selector.direction[0] /= norm;
  selector.direction[1] /= norm;
  selector.direction[2] /= norm;
  const char *min_cos = value_after(narg, arg, "min-cos");
  selector.min_cos = min_cos ? std::atof(min_cos) : 0.0;
  selector.enabled = true;
  return selector;
}

bool selected_by_normal(const NormalSelector &selector,
                        const iac::Model::PublicSurfaceTriangle &triangle) {
  if (!selector.enabled) {
    return true;
  }
  const double norm = std::sqrt(triangle.normal[0] * triangle.normal[0] +
                                triangle.normal[1] * triangle.normal[1] +
                                triangle.normal[2] * triangle.normal[2]);
  if (norm <= 0.0) {
    return false;
  }
  const double cos_angle = (triangle.normal[0] * selector.direction[0] +
                            triangle.normal[1] * selector.direction[1] +
                            triangle.normal[2] * selector.direction[2]) /
                           norm;
  return cos_angle >= selector.min_cos;
}

SurfaceValueStats surface_value_stats(
    const std::vector<double> &values,
    const std::vector<iac::Model::PublicSurfaceTriangle> &triangles,
    const NormalSelector &selector) {
  SurfaceValueStats stats;
  for (std::size_t i = 0; i < values.size() && i < triangles.size(); ++i) {
    if (!selected_by_normal(selector, triangles[i])) {
      continue;
    }
    ++stats.selected_count;
    const double area = triangles[i].area;
    stats.selected_area += area;
    if (values[i] <= 0.0) {
      continue;
    }
    ++stats.positive_count;
    stats.positive_area += area;
    stats.positive_value_sum += values[i];
    stats.positive_area_value_sum += values[i] * area;
    stats.max_positive_value = std::max(stats.max_positive_value, values[i]);
  }
  return stats;
}

void set_surface_value_diagnostics(iac::Model &model, const std::string &prefix,
                                   const SurfaceValueStats &stats) {
  model.set_diagnostic(prefix + "-selected-triangles",
                       static_cast<double>(stats.selected_count));
  model.set_diagnostic(prefix + "-selected-area", stats.selected_area);
  model.set_diagnostic(prefix + "-positive-triangles",
                       static_cast<double>(stats.positive_count));
  model.set_diagnostic(prefix + "-positive-area", stats.positive_area);
  model.set_diagnostic(prefix + "-positive-value-sum", stats.positive_value_sum);
  model.set_diagnostic(prefix + "-positive-area-value-sum",
                       stats.positive_area_value_sum);
  model.set_diagnostic(prefix + "-max-positive-value", stats.max_positive_value);
}

void parse_selector(iac::SurfaceFluxCommand &flux, int narg, char **arg, Error *error) {
  const char *select = value_after(narg, arg, "select");
  if (!select) {
    error->all(FLERR, "surface flux source and kinetic/theory modes require select <all|normal|voxels>");
  }
  flux.select = select;
  if (flux.select == "normal") {
    const char *nx = value_after(narg, arg, "nx");
    const char *ny = value_after(narg, arg, "ny");
    const char *nz = value_after(narg, arg, "nz");
    if (!nx || !ny || !nz) {
      error->all(FLERR, "surface flux select normal requires nx, ny, and nz");
    }
    flux.direction[0] = std::atof(nx);
    flux.direction[1] = std::atof(ny);
    flux.direction[2] = std::atof(nz);
    const char *min_cos = value_after(narg, arg, "min-cos");
    if (min_cos) {
      flux.min_cos = std::atof(min_cos);
    }
  } else if (flux.select == "voxels") {
    const char *voxels = value_after(narg, arg, "voxels");
    if (voxels) {
      flux.voxels = voxels;
    }
  }
}

Fix *require_surface_fix(SPARTA *sparta, Modify *modify, Comm *comm, Error *error,
                         const char *command, const char *fix_id, int column) {
  const int ifix = modify->find_fix(fix_id);
  if (ifix < 0) {
    error->all(FLERR, (std::string(command) + " fix ID does not exist").c_str());
  }
  Fix *ave = modify->fix[ifix];
  if (!ave->per_surf_flag) {
    error->all(FLERR, (std::string(command) + " fix must provide per-surf data").c_str());
  }
  const int ncols = ave->size_per_surf_cols == 0 ? 1 : ave->size_per_surf_cols;
  if (column <= 0 || column > ncols) {
    error->all(FLERR, (std::string(command) + " column is out of range").c_str());
  }
  if (ave->size_per_surf_cols == 0 && !ave->vector_surf) {
    error->all(FLERR, (std::string(command) + " fix vector data is not available").c_str());
  }
  if (ave->size_per_surf_cols > 0 && !ave->array_surf) {
    error->all(FLERR, (std::string(command) + " fix array data is not available").c_str());
  }
  (void)sparta;
  return ave;
}

Fix *require_global_vector_fix(Modify *modify, Error *error, const char *command,
                               const char *fix_id, int index) {
  const int ifix = modify->find_fix(fix_id);
  if (ifix < 0) {
    error->all(FLERR, (std::string(command) + " fix ID does not exist").c_str());
  }
  Fix *ave = modify->fix[ifix];
  if (!ave->vector_flag) {
    error->all(FLERR, (std::string(command) + " fix must provide global vector data").c_str());
  }
  if (index < 0 || index >= ave->size_vector) {
    error->all(FLERR, (std::string(command) + " boundary face is out of range for fix vector").c_str());
  }
  return ave;
}

double fix_surface_value(Fix *ave, int local_index, int column) {
  return ave->size_per_surf_cols == 0 ? ave->vector_surf[local_index]
                                      : ave->array_surf[local_index][column - 1];
}

std::vector<double> global_surface_fix_values(SPARTA *sparta, Surf *surf, Fix *ave, int column,
                                              Error *error, const char *command) {
  std::vector<double> local(static_cast<std::size_t>(surf->nsurf), 0.0);
  for (int i = 0; i < surf->nown; ++i) {
    const int triangle_id = surf->distributed ? surf->mytris[i].id : surf->tris[i].id;
    if (triangle_id <= 0 || triangle_id > static_cast<int>(local.size())) {
      error->all(FLERR, (std::string(command) + " encountered invalid surface ID").c_str());
    }
    local[static_cast<std::size_t>(triangle_id - 1)] += fix_surface_value(ave, i, column);
  }
  if (sparta->comm->nprocs == 1) {
    return local;
  }
  std::vector<double> global(local.size(), 0.0);
  MPI_Allreduce(local.data(), global.data(), static_cast<int>(local.size()), MPI_DOUBLE,
                MPI_SUM, sparta->world);
  return global;
}

void set_internal_variable(Input *input, Error *error, const char *name, double value) {
  Variable *variable = input->variable;
  int ivar = variable->find(const_cast<char *>(name));
  if (ivar < 0) {
    char command[512];
    std::snprintf(command, sizeof(command), "variable %s internal %.17g", name, value);
    input->one(command);
    return;
  }
  if (!variable->internal_style(ivar)) {
    std::string message = "IAC variable '" + std::string(name) + "' must be internal style";
    error->all(FLERR, message.c_str());
  }
  variable->internal_set(ivar, value);
}

double reduce_surface_values(const std::vector<double> &values,
                             const std::vector<iac::Model::PublicSurfaceTriangle> &triangles,
                             const std::string &mode, const NormalSelector &selector,
                             Error *error, const char *command) {
  if (values.size() != triangles.size()) {
    error->all(FLERR, (std::string(command) + " triangle count mismatch").c_str());
  }
  double sum = 0.0;
  double area_sum = 0.0;
  std::size_t selected_count = 0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (!selected_by_normal(selector, triangles[i])) {
      continue;
    }
    ++selected_count;
    const double area = triangles[i].area;
    area_sum += area;
    if (mode == "sum-area" || mode == "ave-area") {
      sum += values[i] * area;
    } else if (mode == "sum" || mode == "ave") {
      sum += values[i];
    } else {
      error->all(FLERR, (std::string(command) + " reduce must be sum, ave, sum-area, or ave-area").c_str());
    }
  }
  if (mode == "ave") {
    return selected_count > 0 ? sum / static_cast<double>(selected_count) : 0.0;
  }
  if (mode == "ave-area") {
    return area_sum > 0.0 ? sum / area_sum : 0.0;
  }
  return sum;
}

void run_dsmc_block(SPARTA *sparta, int steps, bool first_block) {
  std::vector<std::string> strings;
  strings.push_back(std::to_string(steps));
  if (!first_block) {
    strings.push_back("pre");
    strings.push_back("no");
  }
  strings.push_back("post");
  strings.push_back("no");
  std::vector<char *> args;
  args.reserve(strings.size());
  for (auto &text : strings) {
    args.push_back(const_cast<char *>(text.c_str()));
  }
  Run run(sparta);
  run.command(static_cast<int>(args.size()), args.data());
}

void reduce_surface_fields(SPARTA *sparta, Surf *surf,
                           std::vector<iac::Model::SurfaceCellField> &fields) {
  if (sparta->comm->nprocs == 1) {
    return;
  }
  for (auto &field : fields) {
    std::vector<double> global(field.values.size(), 0.0);
    MPI_Allreduce(field.values.data(), global.data(), static_cast<int>(field.values.size()),
                  MPI_DOUBLE, MPI_SUM, sparta->world);
    field.values.swap(global);
  }
  (void)surf;
}

int parse_dsmc_surf_quantity(const char *quantity, Error *error) {
  if (!quantity) {
    return 0;
  }
  if (std::strcmp(quantity, "incident-number-flux") == 0 ||
      std::strcmp(quantity, "nflux-incident") == 0 ||
      std::strcmp(quantity, "nflux_incident") == 0) {
    return 1;
  }
  error->all(FLERR, "surface flux dsmc/surf quantity must be incident-number-flux");
  return 0;
}

int boundary_face_index(const char *face, Error *error, const char *command) {
  if (std::strcmp(face, "xlo") == 0) {
    return 0;
  }
  if (std::strcmp(face, "xhi") == 0) {
    return 1;
  }
  if (std::strcmp(face, "ylo") == 0) {
    return 2;
  }
  if (std::strcmp(face, "yhi") == 0) {
    return 3;
  }
  if (std::strcmp(face, "zlo") == 0) {
    return 4;
  }
  if (std::strcmp(face, "zhi") == 0) {
    return 5;
  }
  error->all(FLERR, (std::string(command) +
                     " boundary must be xlo, xhi, ylo, yhi, zlo, or zhi").c_str());
  return -1;
}

std::string trim(const std::string &text) {
  const std::size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

std::string next_data_line(std::ifstream &input) {
  std::string line;
  while (std::getline(input, line)) {
    line = trim(line);
    if (!line.empty() && line[0] != '#') {
      return line;
    }
  }
  return "";
}

std::map<std::string, double> parse_formula(const std::string &formula) {
  std::map<std::string, double> counts;
  for (std::size_t i = 0; i < formula.size();) {
    if (!std::isupper(static_cast<unsigned char>(formula[i]))) {
      throw std::runtime_error("reaction file contains an unsupported species formula '" + formula + "'");
    }
    std::string element(1, formula[i++]);
    while (i < formula.size() && std::islower(static_cast<unsigned char>(formula[i]))) {
      element.push_back(formula[i++]);
    }
    std::string digits;
    while (i < formula.size() && std::isdigit(static_cast<unsigned char>(formula[i]))) {
      digits.push_back(formula[i++]);
    }
    counts[element] += digits.empty() ? 1.0 : std::atof(digits.c_str());
  }
  return counts;
}

void add_formula_counts(std::map<std::string, double> &total,
                        const std::string &formula, double sign) {
  const auto counts = parse_formula(formula);
  for (const auto &entry : counts) {
    total[entry.first] += sign * entry.second;
  }
}

double solid_formula_mass(const iac::Material &material) {
  if (material.formula.empty() || material.molar_mass <= 0.0) {
    throw std::runtime_error(
        "solid mass inference requires voxel material formula and molar-mass");
  }
  const auto solid_counts = parse_formula(material.formula);
  if (solid_counts.size() != 1) {
    throw std::runtime_error(
        "solid mass inference currently requires a one-element solid formula");
  }
  return material.molar_mass / AVOGADRO;
}

ReactionFileInfo parse_reaction_file(const char *path, const iac::Material &material) {
  if (material.formula.empty() || material.molar_mass <= 0.0) {
    throw std::runtime_error(
        "reaction-derived solid mass requires voxel material formula and molar-mass");
  }

  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error(std::string("Cannot open reaction file ") + path);
  }
  const std::string formula_line = next_data_line(input);
  const std::string coeff_line = next_data_line(input);
  if (formula_line.empty() || coeff_line.empty()) {
    throw std::runtime_error("reaction file must contain a SPARTA reaction formula and coefficient line");
  }

  std::map<std::string, double> gas_delta;
  std::istringstream formula_stream(formula_line);
  std::string token;
  bool products = false;
  while (formula_stream >> token) {
    if (token == "+") {
      continue;
    }
    if (token == "-->") {
      products = true;
      continue;
    }
    if (token == "NULL") {
      continue;
    }
    add_formula_counts(gas_delta, token, products ? 1.0 : -1.0);
  }
  if (!products) {
    throw std::runtime_error("reaction file formula is missing -->");
  }

  const auto solid_counts = parse_formula(material.formula);
  if (solid_counts.size() != 1) {
    throw std::runtime_error("reaction-derived solid mass currently requires a one-element solid formula");
  }
  const auto solid = *solid_counts.begin();
  const auto needed = gas_delta.find(solid.first);
  if (needed == gas_delta.end() || needed->second <= 0.0) {
    throw std::runtime_error(
        "reaction file does not require positive consumption of the voxel material element");
  }
  const double solid_formula_units = needed->second / solid.second;

  std::istringstream coeff_stream(coeff_line);
  std::string type;
  std::string style;
  double probability = 0.0;
  coeff_stream >> type >> style >> probability;
  if (type.empty() || style.empty() || probability <= 0.0 || probability > 1.0) {
    throw std::runtime_error("reaction file probability must be in (0,1]");
  }

  ReactionFileInfo info;
  info.probability = probability;
  info.solid_mass = solid_formula_units * material.molar_mass / AVOGADRO;
  return info;
}

void forward_surface(SPARTA *sparta, const char *subcommand, int narg, char **arg) {
  std::vector<char *> forwarded;
  forwarded.reserve(static_cast<std::size_t>(narg) + 1);
  forwarded.push_back(const_cast<char *>(subcommand));
  for (int i = 0; i < narg; ++i) {
    forwarded.push_back(arg[i]);
  }
  Surface surface(sparta);
  surface.command(static_cast<int>(forwarded.size()), forwarded.data());
}

} // namespace

Surface::Surface(SPARTA *sparta) : Pointers(sparta) {}

SurfaceDumpCommand::SurfaceDumpCommand(SPARTA *sparta) : Pointers(sparta) {}
void SurfaceDumpCommand::command(int narg, char **arg) {
  forward_surface(sparta, "dump", narg, arg);
}

DsmcConverge::DsmcConverge(SPARTA *sparta) : Pointers(sparta) {}
void DsmcConverge::command(int narg, char **arg) {
  forward_surface(sparta, "converge", narg, arg);
}

SurfaceFlux::SurfaceFlux(SPARTA *sparta) : Pointers(sparta) {}
void SurfaceFlux::command(int narg, char **arg) { forward_surface(sparta, "flux", narg, arg); }

SurfaceInstall::SurfaceInstall(SPARTA *sparta) : Pointers(sparta) {}
void SurfaceInstall::command(int narg, char **arg) {
  forward_surface(sparta, "install", narg, arg);
}

SurfaceMeasureFlux::SurfaceMeasureFlux(SPARTA *sparta) : Pointers(sparta) {}
void SurfaceMeasureFlux::command(int narg, char **arg) {
  forward_surface(sparta, "measure-flux", narg, arg);
}

SurfaceWriteVtp::SurfaceWriteVtp(SPARTA *sparta) : Pointers(sparta) {}
void SurfaceWriteVtp::command(int narg, char **arg) {
  forward_surface(sparta, "write-vtp", narg, arg);
}

void Surface::command(int narg, char **arg) {
  if (narg < 1) {
    error->all(FLERR, "Illegal surface command");
  }

  if (std::strcmp(arg[0], "flux") == 0) {
    if (narg < 5) {
      error->all(FLERR, "Illegal surface flux command");
    }
    iac::SurfaceFluxCommand flux;
    flux.surface = arg[1];

    if (std::strcmp(arg[2], "dsmc/mass-flux") == 0) {
      char **pairs = arg + 3;
      const int npairs = narg - 3;
      const char *fix_id = value_after(npairs, pairs, "fix");
      const char *units_value = value_after(npairs, pairs, "units");
      const NormalSelector selector =
          parse_optional_normal_selector(npairs, pairs, error, "surface flux dsmc/mass-flux");
      if (!fix_id || !units_value) {
        error->all(FLERR, "surface flux dsmc/mass-flux requires fix, quantity mass-flux, and units <flux|flow>");
      }
      const bool units_flux = std::strcmp(units_value, "flux") == 0;
      const bool units_flow = std::strcmp(units_value, "flow") == 0;
      if (!units_flux && !units_flow) {
        error->all(FLERR, "surface flux dsmc/mass-flux units must be flux or flow");
      }
      const int column = direct_mass_flux_column(npairs, pairs, error,
                                                 "surface flux dsmc/mass-flux");
      Fix *ave = require_surface_fix(sparta, modify, comm, error,
                                     "surface flux dsmc/mass-flux", fix_id, column);
      const char *ablation_dt_value = value_after(npairs, pairs, "ablation-dt");
      const char *mass_courant_value = value_after(npairs, pairs, "mass-courant");
      const double ablation_dt = ablation_dt_value ? std::atof(ablation_dt_value) : 0.0;
      if (ablation_dt_value && ablation_dt <= 0.0) {
        error->all(FLERR, "surface flux dsmc/mass-flux ablation-dt must be positive");
      }
      const double mass_courant = mass_courant_value ? std::atof(mass_courant_value) : 0.0;
      if (mass_courant_value && mass_courant <= 0.0) {
        error->all(FLERR, "surface flux dsmc/mass-flux mass-courant must be positive");
      }
      if (ablation_dt_value && mass_courant_value) {
        error->all(FLERR, "surface flux dsmc/mass-flux cannot use both ablation-dt and mass-courant");
      }

      std::string root_error;
      auto mass_fluxes =
          global_surface_fix_values(sparta, surf, ave, column, error,
                                    "surface flux dsmc/mass-flux");
      try {
        if (IACBridge::owns_model(sparta)) {
          const auto &triangles = IACBridge::surface_triangles(sparta, flux.surface);
          if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
            throw std::runtime_error("surface flux dsmc/mass-flux triangle count does not match SPARTA surface count");
          }
          for (std::size_t idx = 0; idx < mass_fluxes.size(); ++idx) {
            if (!selected_by_normal(selector, triangles[idx])) {
              mass_fluxes[idx] = 0.0;
            } else if (units_flow) {
              const double area = triangles[idx].area;
              mass_fluxes[idx] = area > 0.0 ? mass_fluxes[idx] / area : 0.0;
            }
          }
          set_surface_value_diagnostics(
              IACBridge::model(sparta), "dsmc-mass-flux",
              surface_value_stats(mass_fluxes, triangles, selector));
          if (mass_courant_value) {
            IACBridge::model(sparta).set_timestep_from_triangle_fluxes(
                flux.surface, mass_fluxes, mass_courant);
          } else if (ablation_dt_value) {
            IACBridge::model(sparta).set_timestep(ablation_dt);
          } else {
            IACBridge::set_coupling_interval_from_dsmc(sparta);
          }
          IACBridge::model(sparta).apply_triangle_fluxes(flux.surface, mass_fluxes);
        }
      } catch (const std::exception &ex) {
        root_error = ex.what();
      }
      IACBridge::error_if_root_failed(sparta, root_error);
      return;
    }

    if (std::strcmp(arg[2], "dsmc/reaction") == 0) {
      char **pairs = arg + 3;
      const int npairs = narg - 3;
      const char *fix_id = value_after(npairs, pairs, "fix");
      const char *column_value = value_after(npairs, pairs, "column");
      const char *sample_steps_value = value_after(npairs, pairs, "sample-steps");
      const char *solid_mass = value_after(npairs, pairs, "solid-mass-per-reaction");
      const char *reaction_file = value_after(npairs, pairs, "reaction");
      const NormalSelector selector =
          parse_optional_normal_selector(npairs, pairs, error, "surface flux dsmc/reaction");
      if (!fix_id || !column_value || !sample_steps_value || (!solid_mass && !reaction_file)) {
        error->all(FLERR, "surface flux dsmc/reaction requires fix, column, sample-steps, and reaction or solid-mass-per-reaction");
      }
      if (solid_mass && reaction_file) {
        error->all(FLERR, "surface flux dsmc/reaction cannot use both reaction and solid-mass-per-reaction");
      }

      const int ifix = modify->find_fix(fix_id);
      if (ifix < 0) {
        error->all(FLERR, "surface flux dsmc/reaction fix ID does not exist");
      }
      Fix *ave = modify->fix[ifix];
      if (!ave->per_surf_flag) {
        error->all(FLERR, "surface flux dsmc/reaction fix must provide per-surf data");
      }

      const int column = std::atoi(column_value);
      const int ncols = ave->size_per_surf_cols == 0 ? 1 : ave->size_per_surf_cols;
      if (column <= 0 || column > ncols) {
        error->all(FLERR, "surface flux dsmc/reaction column is out of range");
      }
      if (ave->size_per_surf_cols == 0 && !ave->vector_surf) {
        error->all(FLERR, "surface flux dsmc/reaction fix vector data is not available");
      }
      if (ave->size_per_surf_cols > 0 && !ave->array_surf) {
        error->all(FLERR, "surface flux dsmc/reaction fix array data is not available");
      }
      const int sample_steps = std::atoi(sample_steps_value);
      const char *ablation_dt_value = value_after(npairs, pairs, "ablation-dt");
      const char *mass_courant_value = value_after(npairs, pairs, "mass-courant");
      const char *time_scale_value = value_after(npairs, pairs, "time-scale");
      double solid_mass_per_reaction = solid_mass ? std::atof(solid_mass) : 0.0;
      if (reaction_file) {
        try {
          solid_mass_per_reaction =
              parse_reaction_file(reaction_file, IACBridge::config().material).solid_mass;
        } catch (const std::exception &ex) {
          error->all(FLERR, ex.what());
        }
      }
      if (sample_steps <= 0 || solid_mass_per_reaction <= 0.0) {
        error->all(FLERR, "surface flux dsmc/reaction has invalid sample-steps or solid-mass-per-reaction");
      }
      const double time_scale = time_scale_value ? std::atof(time_scale_value) : 1.0;
      if (time_scale <= 0.0) {
        error->all(FLERR, "surface flux dsmc/reaction time-scale must be positive");
      }
      const double ablation_dt = ablation_dt_value ? std::atof(ablation_dt_value) : 0.0;
      if (ablation_dt_value && ablation_dt <= 0.0) {
        error->all(FLERR, "surface flux dsmc/reaction ablation-dt must be positive");
      }
      const double mass_courant = mass_courant_value ? std::atof(mass_courant_value) : 0.0;
      if (mass_courant_value && mass_courant <= 0.0) {
        error->all(FLERR, "surface flux dsmc/reaction mass-courant must be positive");
      }
      if (ablation_dt_value && mass_courant_value) {
        error->all(FLERR, "surface flux dsmc/reaction cannot use both ablation-dt and mass-courant");
      }

      std::string root_error;
      const auto reaction_counts =
          global_surface_fix_values(sparta, surf, ave, column, error,
                                    "surface flux dsmc/reaction");
      try {
        if (IACBridge::owns_model(sparta)) {
          const auto &triangles = IACBridge::surface_triangles(sparta, flux.surface);
          if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
            throw std::runtime_error("surface flux dsmc/reaction triangle count does not match SPARTA surface count");
          }
          std::vector<double> mass_fluxes(static_cast<std::size_t>(surf->nsurf), 0.0);
          bool has_positive_flux = false;
          const double sample_dt = static_cast<double>(sample_steps) * update->dt * time_scale;
          for (std::size_t idx = 0; idx < mass_fluxes.size(); ++idx) {
            const double area = triangles[idx].area;
            if (area > 0.0 && selected_by_normal(selector, triangles[idx])) {
              const double mass = reaction_counts[idx] * static_cast<double>(sample_steps) *
                                  update->fnum * solid_mass_per_reaction * time_scale;
              mass_fluxes[idx] = mass / (area * sample_dt);
              if (mass_fluxes[idx] > 0.0) {
                has_positive_flux = true;
              }
            }
          }
          if (mass_courant_value) {
            if (has_positive_flux) {
              IACBridge::model(sparta).set_timestep_from_triangle_fluxes(
                  flux.surface, mass_fluxes, mass_courant);
            } else {
              IACBridge::model(sparta).set_timestep(sample_dt);
              IACBridge::model(sparta).set_diagnostic("surface-mass-courant", mass_courant);
              IACBridge::model(sparta).set_diagnostic("surface-max-face-flux", 0.0);
              IACBridge::model(sparta).set_diagnostic("surface-mass-courant-dt", sample_dt);
            }
          } else if (ablation_dt_value) {
            IACBridge::model(sparta).set_timestep(ablation_dt);
          } else {
            IACBridge::model(sparta).set_timestep(static_cast<double>(sample_steps) *
                                                  update->dt * time_scale);
            IACBridge::set_last_coupling_step(sparta);
          }
          IACBridge::model(sparta).apply_triangle_fluxes(flux.surface, mass_fluxes);
        }
      } catch (const std::exception &ex) {
        root_error = ex.what();
      }
      IACBridge::error_if_root_failed(sparta, root_error);
      return;
    }

    if (std::strcmp(arg[2], "dsmc/surf") == 0) {
      char **pairs = arg + 3;
      const int npairs = narg - 3;
      const char *fix_id = value_after(npairs, pairs, "fix");
      const char *column_value = value_after(npairs, pairs, "column");
      const char *quantity_value = value_after(npairs, pairs, "quantity");
      const char *solid_mass = value_after(npairs, pairs, "solid-mass-per-hit");
      const char *reaction_file = value_after(npairs, pairs, "reaction");
      if (!fix_id) {
        error->all(FLERR, "surface flux dsmc/surf requires fix");
      }
      if (column_value && quantity_value) {
        error->all(FLERR, "surface flux dsmc/surf cannot use both column and quantity");
      }
      if (solid_mass && reaction_file) {
        error->all(FLERR, "surface flux dsmc/surf cannot use both reaction and solid-mass-per-hit");
      }

      const int ifix = modify->find_fix(fix_id);
      if (ifix < 0) {
        error->all(FLERR, "surface flux dsmc/surf fix ID does not exist");
      }
      Fix *ave = modify->fix[ifix];
      if (!ave->per_surf_flag) {
        error->all(FLERR, "surface flux dsmc/surf fix must provide per-surf data");
      }

      const int column = column_value ? std::atoi(column_value)
                                      : parse_dsmc_surf_quantity(
                                            quantity_value ? quantity_value
                                                           : "incident-number-flux",
                                            error);
      const int ncols = ave->size_per_surf_cols == 0 ? 1 : ave->size_per_surf_cols;
      if (column <= 0 || column > ncols) {
        error->all(FLERR, "surface flux dsmc/surf column is out of range");
      }
      if (ave->size_per_surf_cols == 0 && !ave->vector_surf) {
        error->all(FLERR, "surface flux dsmc/surf fix vector data is not available");
      }
      if (ave->size_per_surf_cols > 0 && !ave->array_surf) {
        error->all(FLERR, "surface flux dsmc/surf fix array data is not available");
      }
      const char *reaction_prob_value = value_after(npairs, pairs, "reaction-prob");
      const char *ablation_dt_value = value_after(npairs, pairs, "ablation-dt");
      const char *mass_courant_value = value_after(npairs, pairs, "mass-courant");
      double reaction_prob = reaction_prob_value ? std::atof(reaction_prob_value) : 1.0;
      double solid_mass_per_hit = solid_mass ? std::atof(solid_mass) : 0.0;
      if (reaction_file) {
        try {
          const auto reaction = parse_reaction_file(reaction_file, IACBridge::config().material);
          solid_mass_per_hit = reaction.solid_mass;
          reaction_prob = reaction.probability;
        } catch (const std::exception &ex) {
          error->all(FLERR, ex.what());
        }
      } else if (!solid_mass) {
        try {
          solid_mass_per_hit = solid_formula_mass(IACBridge::config().material);
        } catch (const std::exception &ex) {
          error->all(FLERR, ex.what());
        }
      }
      if (reaction_prob < 0.0 || reaction_prob > 1.0 || solid_mass_per_hit <= 0.0) {
        error->all(FLERR, "surface flux dsmc/surf has invalid reaction-prob or solid-mass-per-hit");
      }
      const double ablation_dt = ablation_dt_value ? std::atof(ablation_dt_value) : 0.0;
      if (ablation_dt_value && ablation_dt <= 0.0) {
        error->all(FLERR, "surface flux dsmc/surf ablation-dt must be positive");
      }
      const double mass_courant = mass_courant_value ? std::atof(mass_courant_value) : 0.0;
      if (mass_courant_value && mass_courant <= 0.0) {
        error->all(FLERR, "surface flux dsmc/surf mass-courant must be positive");
      }
      if (ablation_dt_value && mass_courant_value) {
        error->all(FLERR, "surface flux dsmc/surf cannot use both ablation-dt and mass-courant");
      }

      auto mass_fluxes =
          global_surface_fix_values(sparta, surf, ave, column, error,
                                    "surface flux dsmc/surf");
      for (double &mass_flux : mass_fluxes) {
        mass_flux *= reaction_prob * solid_mass_per_hit;
      }

      std::string root_error;
      try {
        if (IACBridge::owns_model(sparta)) {
          if (mass_courant_value) {
            IACBridge::model(sparta).set_timestep_from_triangle_fluxes(
                flux.surface, mass_fluxes, mass_courant);
          } else if (ablation_dt_value) {
            IACBridge::model(sparta).set_timestep(ablation_dt);
          } else {
            IACBridge::set_coupling_interval_from_dsmc(sparta);
          }
          IACBridge::model(sparta).apply_triangle_fluxes(flux.surface, mass_fluxes);
        }
      } catch (const std::exception &ex) {
        root_error = ex.what();
      }
      IACBridge::error_if_root_failed(sparta, root_error);
      return;
    } else if (std::strcmp(arg[2], "kinetic/theory") == 0) {
      flux.style = "kinetic/theory";
      char **pairs = arg + 3;
      const int npairs = narg - 3;
      const char *pressure = value_after(npairs, pairs, "pressure");
      const char *temperature = value_after(npairs, pairs, "temperature");
      const char *molecular_mass = value_after(npairs, pairs, "molecular-mass");
      const char *solid_mass = value_after(npairs, pairs, "solid-mass-per-hit");
      if (!pressure || !temperature || !molecular_mass || !solid_mass) {
        error->all(FLERR, "surface flux kinetic/theory is missing required parameters");
      }
      flux.pressure = std::atof(pressure);
      flux.temperature = std::atof(temperature);
      flux.molecular_mass = std::atof(molecular_mass);
      flux.solid_mass_per_hit = std::atof(solid_mass);
      const char *mole_fraction = value_after(npairs, pairs, "mole-fraction");
      const char *reaction_prob = value_after(npairs, pairs, "reaction-prob");
      if (mole_fraction) {
        flux.mole_fraction = std::atof(mole_fraction);
      }
      if (reaction_prob) {
        flux.reaction_prob = std::atof(reaction_prob);
      }
      parse_selector(flux, npairs, pairs, error);
    } else if (std::strcmp(arg[2], "source") == 0) {
      if (narg < 4) {
        error->all(FLERR, "surface flux source requires a source id");
      }
      flux.style = "source";
      flux.source = arg[3];
      parse_selector(flux, narg - 4, arg + 4, error);
    } else {
      error->all(FLERR, "surface flux style must be source or kinetic/theory");
    }

    try {
      if (IACBridge::owns_model(sparta)) {
        IACBridge::model(sparta).apply_flux(flux);
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "converge") == 0) {
    if (narg < 3 || std::strcmp(arg[1], "flux") != 0) {
      error->all(FLERR, "Illegal dsmc_converge command");
    }
    if (std::strcmp(arg[2], "boundary") == 0) {
      if (narg < 18) {
        error->all(FLERR, "Illegal dsmc_converge command");
      }
      const int face_index = boundary_face_index(arg[3], error, "dsmc_converge flux boundary");
      char **pairs = arg + 4;
      const int npairs = narg - 4;
      const char *fix_id = value_after(npairs, pairs, "fix");
      const char *quantity_value = value_after(npairs, pairs, "quantity");
      const char *every_value = value_after(npairs, pairs, "every");
      const char *rel_value = value_after(npairs, pairs, "rel");
      const char *cv_value = value_after(npairs, pairs, "cv");
      const char *window_value = value_after(npairs, pairs, "window");
      const char *max_iter_value = value_after(npairs, pairs, "max-iter");
      if (!fix_id || !every_value || !rel_value || !cv_value ||
          !window_value || !max_iter_value) {
        error->all(FLERR, "dsmc_converge flux boundary requires boundary, fix, every, rel, cv, window, and max-iter");
      }
      if (quantity_value && std::strcmp(quantity_value, "mass-flux") != 0) {
        error->all(FLERR, "dsmc_converge flux boundary quantity must be mass-flux");
      }
      const int every = std::atoi(every_value);
      const int window_size = std::atoi(window_value);
      const int max_iter = std::atoi(max_iter_value);
      const char *min_iter_value = value_after(npairs, pairs, "min-iter");
      const char *passes_value = value_after(npairs, pairs, "passes");
      const char *variable_name = value_after(npairs, pairs, "variable");
      const bool allow_zero =
          optional_bool_after(npairs, pairs, "allow-zero", false, error,
                              "dsmc_converge flux boundary");
      const int min_iter = min_iter_value ? std::atoi(min_iter_value) : window_size;
      const int passes_required = passes_value ? std::atoi(passes_value) : 1;
      const double rel_tol = std::atof(rel_value);
      const double cv_tol = std::atof(cv_value);
      if (every <= 0 || window_size <= 0 || max_iter <= 0 || min_iter <= 0 ||
          passes_required <= 0 || rel_tol < 0.0 || cv_tol < 0.0) {
        error->all(FLERR, "dsmc_converge flux boundary has invalid numeric arguments");
      }

      ConvergenceWindow window;
      window.max_size = window_size;
      bool converged = false;
      int pass_count = 0;
      double previous = 0.0;
      double current = 0.0;
      double rel = 0.0;
      double cv = 0.0;
      bool has_positive_support = false;
      bool warned_zero_support = false;
      int positive_support_iters = 0;
      int zero_support_iters = 0;
      int iter = 0;

      IACBridge::print_coupled_summary(sparta);

      for (iter = 1; iter <= max_iter; ++iter) {
        run_dsmc_block(sparta, every, iter == 1);
        Fix *ave = require_global_vector_fix(modify, error, "dsmc_converge flux boundary",
                                             fix_id, face_index);
        current = ave->compute_vector(face_index);
        has_positive_support = current > 0.0;
        if (has_positive_support) {
          ++positive_support_iters;
        } else {
          ++zero_support_iters;
          if (!allow_zero && !warned_zero_support && comm->me == 0) {
            error->warning(
                FLERR,
                "dsmc_converge flux boundary sampled no positive selected mass flux; "
                "zero-flux windows are not considered converged unless allow-zero yes");
          }
          warned_zero_support = true;
        }
        rel = iter == 1 ? 1.0 : std::abs(current - previous) /
                                  std::sqrt(previous * previous + 1.0e-300);
        window.push(current);
        cv = window.cv();
        const bool pass = (allow_zero || has_positive_support) &&
                          iter >= min_iter && window.full() &&
                          rel <= rel_tol && cv <= cv_tol;
        pass_count = pass ? pass_count + 1 : 0;
        if (pass_count >= passes_required) {
          converged = true;
          break;
        }
        previous = current;
      }

      if (IACBridge::owns_model(sparta)) {
        auto &model = IACBridge::model(sparta);
        model.set_diagnostic("dsmc-converge-value", current);
        model.set_diagnostic("dsmc-converge-rel", rel);
        model.set_diagnostic("dsmc-converge-cv", cv);
        model.set_diagnostic("dsmc-converge-iter", static_cast<double>(iter));
        model.set_diagnostic("dsmc-converge-steps", static_cast<double>(iter * every));
        model.set_diagnostic("dsmc-converged", converged ? 1.0 : 0.0);
        model.set_diagnostic("dsmc-converge-positive-support",
                             has_positive_support ? 1.0 : 0.0);
        model.set_diagnostic("dsmc-converge-positive-support-iters",
                             static_cast<double>(positive_support_iters));
        model.set_diagnostic("dsmc-converge-zero-support-iters",
                             static_cast<double>(zero_support_iters));
      }
      if (variable_name) {
        set_internal_variable(input, error, variable_name, converged ? 1.0 : 0.0);
      }
      if (!converged) {
        if (!allow_zero && positive_support_iters == 0 && zero_support_iters > 0) {
          error->all(FLERR,
                     "dsmc_converge flux boundary did not converge before max-iter: "
                     "sampled no positive selected mass flux; increase sampling, "
                     "check species/reaction inputs, or set allow-zero yes if zero "
                     "flux is expected");
        }
        error->all(FLERR, "dsmc_converge flux boundary did not converge before max-iter");
      }
      return;
    }
    if (narg < 19) {
      error->all(FLERR, "Illegal dsmc_converge command");
    }
    const char *surface_id = arg[2];
    char **pairs = arg + 3;
    const int npairs = narg - 3;
    const char *fix_id = value_after(npairs, pairs, "fix");
    const char *every_value = value_after(npairs, pairs, "every");
    const char *reduce_value = value_after(npairs, pairs, "reduce");
    const char *rel_value = value_after(npairs, pairs, "rel");
    const char *cv_value = value_after(npairs, pairs, "cv");
    const char *window_value = value_after(npairs, pairs, "window");
    const char *max_iter_value = value_after(npairs, pairs, "max-iter");
    if (!fix_id || !every_value || !reduce_value || !rel_value ||
        !cv_value || !window_value || !max_iter_value) {
      error->all(FLERR, "dsmc_converge flux requires surface, fix, quantity mass-flux, every, reduce, rel, cv, window, and max-iter");
    }
    const int column = direct_mass_flux_column(npairs, pairs, error, "dsmc_converge flux");
    const NormalSelector selector =
        parse_optional_normal_selector(npairs, pairs, error, "dsmc_converge flux");
    const int every = std::atoi(every_value);
    const int window_size = std::atoi(window_value);
    const int max_iter = std::atoi(max_iter_value);
    const char *min_iter_value = value_after(npairs, pairs, "min-iter");
    const char *passes_value = value_after(npairs, pairs, "passes");
    const char *variable_name = value_after(npairs, pairs, "variable");
    const bool allow_zero =
        optional_bool_after(npairs, pairs, "allow-zero", false, error,
                            "dsmc_converge flux");
    const int min_iter = min_iter_value ? std::atoi(min_iter_value) : window_size;
    const int passes_required = passes_value ? std::atoi(passes_value) : 1;
    const double rel_tol = std::atof(rel_value);
    const double cv_tol = std::atof(cv_value);
    if (every <= 0 || column <= 0 || window_size <= 0 || max_iter <= 0 ||
        min_iter <= 0 || passes_required <= 0 || rel_tol < 0.0 || cv_tol < 0.0) {
      error->all(FLERR, "dsmc_converge flux has invalid numeric arguments");
    }

    ConvergenceWindow window;
    window.max_size = window_size;
    bool converged = false;
    int pass_count = 0;
    double previous = 0.0;
    double current = 0.0;
    double rel = 0.0;
    double cv = 0.0;
    bool has_positive_support = false;
    bool warned_zero_support = false;
    int positive_support_iters = 0;
    int zero_support_iters = 0;
    int iter = 0;

    IACBridge::print_coupled_summary(sparta);

    for (iter = 1; iter <= max_iter; ++iter) {
      run_dsmc_block(sparta, every, iter == 1);
      Fix *ave = require_surface_fix(sparta, modify, comm, error,
                                     "dsmc_converge flux", fix_id, column);
      const auto values =
          global_surface_fix_values(sparta, surf, ave, column, error,
                                    "dsmc_converge flux");
      std::string root_error;
      try {
        if (IACBridge::owns_model(sparta)) {
          const auto &triangles = IACBridge::surface_triangles(sparta, surface_id);
          if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
            throw std::runtime_error("dsmc_converge flux triangle count does not match SPARTA surface count");
          }
          const SurfaceValueStats stats = surface_value_stats(values, triangles, selector);
          set_surface_value_diagnostics(IACBridge::model(sparta), "dsmc-converge",
                                        stats);
          has_positive_support = stats.positive_count > 0 &&
                                 stats.positive_area_value_sum > 0.0;
          current = reduce_surface_values(values, triangles, reduce_value, selector, error,
                                          "dsmc_converge flux");
        }
      } catch (const std::exception &ex) {
        root_error = ex.what();
      }
      IACBridge::error_if_root_failed(sparta, root_error);
      MPI_Bcast(&current, 1, MPI_DOUBLE, 0, sparta->world);
      int positive_support_flag = has_positive_support ? 1 : 0;
      MPI_Bcast(&positive_support_flag, 1, MPI_INT, 0, sparta->world);
      has_positive_support = positive_support_flag != 0;
      if (has_positive_support) {
        ++positive_support_iters;
      } else {
        ++zero_support_iters;
        if (!allow_zero && !warned_zero_support && comm->me == 0) {
          error->warning(
              FLERR,
              "dsmc_converge flux sampled no positive selected mass flux; "
              "zero-flux windows are not considered converged unless allow-zero yes");
        }
        warned_zero_support = true;
      }
      rel = iter == 1 ? 1.0 : std::abs(current - previous) /
                                std::sqrt(previous * previous + 1.0e-300);
      window.push(current);
      cv = window.cv();
      const bool pass = (allow_zero || has_positive_support) &&
                        iter >= min_iter && window.full() &&
                        rel <= rel_tol && cv <= cv_tol;
      pass_count = pass ? pass_count + 1 : 0;
      if (pass_count >= passes_required) {
        converged = true;
        break;
      }
      previous = current;
    }

    if (IACBridge::owns_model(sparta)) {
      auto &model = IACBridge::model(sparta);
      model.set_diagnostic("dsmc-converge-value", current);
      model.set_diagnostic("dsmc-converge-rel", rel);
      model.set_diagnostic("dsmc-converge-cv", cv);
      model.set_diagnostic("dsmc-converge-iter", static_cast<double>(iter));
      model.set_diagnostic("dsmc-converge-steps", static_cast<double>(iter * every));
      model.set_diagnostic("dsmc-converged", converged ? 1.0 : 0.0);
      model.set_diagnostic("dsmc-converge-positive-support",
                           has_positive_support ? 1.0 : 0.0);
      model.set_diagnostic("dsmc-converge-positive-support-iters",
                           static_cast<double>(positive_support_iters));
      model.set_diagnostic("dsmc-converge-zero-support-iters",
                           static_cast<double>(zero_support_iters));
    }
    if (variable_name) {
      set_internal_variable(input, error, variable_name, converged ? 1.0 : 0.0);
    }
    if (!converged) {
      if (!allow_zero && positive_support_iters == 0 && zero_support_iters > 0) {
        error->all(FLERR,
                   "dsmc_converge flux did not converge before max-iter: "
                   "sampled no positive selected mass flux; increase sampling, "
                   "check species/reaction inputs, or set allow-zero yes if zero "
                   "flux is expected");
      }
      error->all(FLERR, "dsmc_converge flux did not converge before max-iter");
    }
    return;
  }

  if (std::strcmp(arg[0], "measure-flux") == 0) {
    if (narg >= 12 && std::strcmp(arg[2], "dsmc/mass-flux") == 0) {
      const char *surface_id = arg[1];
      char **pairs = arg + 3;
      const int npairs = narg - 3;
      const char *fix_id = value_after(npairs, pairs, "fix");
      const char *units_value = value_after(npairs, pairs, "units");
      if (!fix_id || !units_value) {
        error->all(FLERR, "surface measure-flux dsmc/mass-flux requires fix, quantity mass-flux, and units <flux|flow>");
      }
      const bool units_flux = std::strcmp(units_value, "flux") == 0;
      const bool units_flow = std::strcmp(units_value, "flow") == 0;
      if (!units_flux && !units_flow) {
        error->all(FLERR, "surface measure-flux dsmc/mass-flux units must be flux or flow");
      }

      const int column = direct_mass_flux_column(npairs, pairs, error,
                                                 "surface measure-flux dsmc/mass-flux");
      Fix *ave = require_surface_fix(sparta, modify, comm, error,
                                     "surface measure-flux dsmc/mass-flux",
                                     fix_id, column);
      const char *expected = value_after(npairs, pairs, "expected");
      const char *number_density_value = value_after(npairs, pairs, "number-density");
      const char *mole_fraction_value = value_after(npairs, pairs, "mole-fraction");
      const char *temperature_value = value_after(npairs, pairs, "temperature");
      const char *molecular_mass_value = value_after(npairs, pairs, "molecular-mass");
      const char *reaction_prob_value = value_after(npairs, pairs, "reaction-prob");
      const char *solid_mass_value = value_after(npairs, pairs, "solid-mass-per-reaction");
      const char *reaction_file = value_after(npairs, pairs, "reaction");
      if (!expected || std::strcmp(expected, "kinetic/theory") != 0 ||
          !number_density_value || !mole_fraction_value || !temperature_value ||
          !molecular_mass_value) {
        error->all(FLERR, "surface measure-flux dsmc/mass-flux expected kinetic/theory requires number-density, mole-fraction, temperature, and molecular-mass");
      }

      const double number_density = std::atof(number_density_value);
      const double mole_fraction = std::atof(mole_fraction_value);
      const double temperature = std::atof(temperature_value);
      const double molecular_mass = std::atof(molecular_mass_value);
      double reaction_prob = reaction_prob_value ? std::atof(reaction_prob_value) : 1.0;
      double solid_mass_per_reaction = solid_mass_value ? std::atof(solid_mass_value) : 0.0;
      if (reaction_file) {
        if (solid_mass_value || reaction_prob_value) {
          error->all(FLERR, "surface measure-flux dsmc/mass-flux cannot use reaction with reaction-prob or solid-mass-per-reaction");
        }
        try {
          const auto reaction = parse_reaction_file(reaction_file, IACBridge::config().material);
          reaction_prob = reaction.probability;
          solid_mass_per_reaction = reaction.solid_mass;
        } catch (const std::exception &ex) {
          error->all(FLERR, ex.what());
        }
      }
      if (number_density <= 0.0 || mole_fraction < 0.0 || temperature <= 0.0 ||
          molecular_mass <= 0.0 || reaction_prob < 0.0 || reaction_prob > 1.0 ||
          solid_mass_per_reaction <= 0.0) {
        error->all(FLERR, "surface measure-flux dsmc/mass-flux expected kinetic/theory has invalid parameters");
      }

      const auto values =
          global_surface_fix_values(sparta, surf, ave, column, error,
                                    "surface measure-flux dsmc/mass-flux");
      std::string root_error;
      try {
        if (!IACBridge::owns_model(sparta)) {
          IACBridge::error_if_root_failed(sparta, root_error);
          return;
        }
        const auto &triangles = IACBridge::surface_triangles(sparta, surface_id);
        if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
          throw std::runtime_error("surface measure-flux dsmc/mass-flux triangle count does not match SPARTA surface count");
        }
        double surface_area = 0.0;
        double mass_flow = 0.0;
        for (std::size_t idx = 0; idx < values.size(); ++idx) {
          const double area = triangles[idx].area;
          surface_area += area;
          mass_flow += units_flux ? values[idx] * area : values[idx];
        }
        const double measured_mass_flux = surface_area > 0.0 ? mass_flow / surface_area : 0.0;
        const double pi = std::acos(-1.0);
        const double expected_number_flux =
            mole_fraction * number_density *
            std::sqrt(1.380649e-23 * temperature / (2.0 * pi * molecular_mass)) *
            reaction_prob;
        const double expected_mass_flux = expected_number_flux * solid_mass_per_reaction;
        double flux_error_percent = 0.0;
        auto &model = IACBridge::model(sparta);
        const auto &cfg = IACBridge::config();
        const double area_exact = cfg.geometry == iac::GeometryKind::Sphere
                                      ? model.diagnostic("surfarea0")
                                      : surface_area;
        model.set_diagnostic("area", surface_area);
        model.set_diagnostic("area-exact", area_exact);
        model.set_diagnostic("rmflux", measured_mass_flux);
        model.set_diagnostic("rmflux-exact", expected_mass_flux);
        model.set_diagnostic("rflux-exact", expected_number_flux);
        flux_error_percent = 100.0 * std::abs(measured_mass_flux - expected_mass_flux) /
                             std::abs(expected_mass_flux);
        model.set_diagnostic("rmflux-errpct", flux_error_percent);
        model.set_diagnostic("area-errpct",
                             100.0 * std::abs(surface_area - area_exact) /
                                 std::abs(area_exact));
        if (screen) {
          std::fprintf(screen,
                       "IAC surface mass flux: measured %.8e expected %.8e error %.6g percent\n",
                       measured_mass_flux, expected_mass_flux, flux_error_percent);
        }
        if (logfile) {
          std::fprintf(logfile,
                       "IAC surface mass flux: measured %.8e expected %.8e error %.6g percent\n",
                       measured_mass_flux, expected_mass_flux, flux_error_percent);
        }
      } catch (const std::exception &ex) {
        root_error = ex.what();
      }
      IACBridge::error_if_root_failed(sparta, root_error);
      return;
    }

    if (narg < 12 || std::strcmp(arg[2], "dsmc/reaction") != 0) {
      error->all(FLERR, "Illegal surface measure-flux command");
    }
    const char *surface_id = arg[1];
    char **pairs = arg + 3;
    const int npairs = narg - 3;
    const char *fix_id = value_after(npairs, pairs, "fix");
    const char *column_value = value_after(npairs, pairs, "column");
    const char *sample_steps_value = value_after(npairs, pairs, "sample-steps");
    if (!fix_id || !column_value || !sample_steps_value) {
      error->all(FLERR, "surface measure-flux dsmc/reaction requires fix, column, and sample-steps");
    }

    const int column = std::atoi(column_value);
    const int sample_steps = std::atoi(sample_steps_value);
    if (sample_steps <= 0) {
      error->all(FLERR, "surface measure-flux dsmc/reaction sample-steps must be positive");
    }
    Fix *ave = require_surface_fix(sparta, modify, comm, error,
                                   "surface measure-flux dsmc/reaction", fix_id, column);

    const char *expected = value_after(npairs, pairs, "expected");
    const char *number_density_value = value_after(npairs, pairs, "number-density");
    const char *mole_fraction_value = value_after(npairs, pairs, "mole-fraction");
    const char *temperature_value = value_after(npairs, pairs, "temperature");
    const char *molecular_mass_value = value_after(npairs, pairs, "molecular-mass");
    const char *reaction_prob_value = value_after(npairs, pairs, "reaction-prob");
    const char *solid_mass_value = value_after(npairs, pairs, "solid-mass-per-reaction");
    const char *reaction_file = value_after(npairs, pairs, "reaction");
    if (!expected || std::strcmp(expected, "kinetic/theory") != 0 ||
        !number_density_value || !mole_fraction_value || !temperature_value ||
        !molecular_mass_value) {
      error->all(FLERR, "surface measure-flux expected kinetic/theory requires number-density, mole-fraction, temperature, and molecular-mass");
    }

    const double number_density = std::atof(number_density_value);
    const double mole_fraction = std::atof(mole_fraction_value);
    const double temperature = std::atof(temperature_value);
    const double molecular_mass = std::atof(molecular_mass_value);
    double reaction_prob = reaction_prob_value ? std::atof(reaction_prob_value) : 1.0;
    double solid_mass_per_reaction = solid_mass_value ? std::atof(solid_mass_value) : 0.0;
    if (reaction_file) {
      if (solid_mass_value || reaction_prob_value) {
        error->all(FLERR, "surface measure-flux cannot use reaction with reaction-prob or solid-mass-per-reaction");
      }
      try {
        const auto reaction = parse_reaction_file(reaction_file, IACBridge::config().material);
        reaction_prob = reaction.probability;
        solid_mass_per_reaction = reaction.solid_mass;
      } catch (const std::exception &ex) {
        error->all(FLERR, ex.what());
      }
    }
    if (number_density <= 0.0 || mole_fraction < 0.0 || temperature <= 0.0 ||
        molecular_mass <= 0.0 || reaction_prob < 0.0 || reaction_prob > 1.0 ||
        (solid_mass_value && solid_mass_per_reaction <= 0.0)) {
      error->all(FLERR, "surface measure-flux expected kinetic/theory has invalid parameters");
    }

    const auto reaction_counts =
        global_surface_fix_values(sparta, surf, ave, column, error,
                                  "surface measure-flux dsmc/reaction");
    std::string root_error;
    try {
      if (!IACBridge::owns_model(sparta)) {
        IACBridge::error_if_root_failed(sparta, root_error);
        return;
      }
      const auto &triangles = IACBridge::surface_triangles(sparta, surface_id);
      if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
        throw std::runtime_error("surface measure-flux triangle count does not match SPARTA surface count");
      }
      double surface_area = 0.0;
      for (const auto &triangle : triangles) {
        surface_area += triangle.area;
      }
      double reaction_count_sum = 0.0;
      for (double value : reaction_counts) {
        reaction_count_sum += value;
      }
      const double measured_flux = reaction_count_sum * update->fnum /
                                   (surface_area * update->dt);
      const double pi = std::acos(-1.0);
      const double expected_flux =
          mole_fraction * number_density *
          std::sqrt(1.380649e-23 * temperature / (2.0 * pi * molecular_mass)) *
          reaction_prob;
      double flux_error_percent = 0.0;
      auto &model = IACBridge::model(sparta);
      const auto &cfg = IACBridge::config();
      const double area_exact = cfg.geometry == iac::GeometryKind::Sphere
                                    ? model.diagnostic("surfarea0")
                                    : surface_area;
      model.set_diagnostic("area", surface_area);
      model.set_diagnostic("area-exact", area_exact);
      model.set_diagnostic("nreact", reaction_count_sum);
      model.set_diagnostic("nreact-exact",
                           expected_flux * surface_area * update->dt / update->fnum);
      model.set_diagnostic("sample-steps", static_cast<double>(sample_steps));
      model.set_diagnostic("rflux", measured_flux);
      model.set_diagnostic("rflux-exact", expected_flux);
      model.set_diagnostic("rflux-ratio", measured_flux / expected_flux);
      flux_error_percent = 100.0 * std::abs(measured_flux - expected_flux) /
                           std::abs(expected_flux);
      model.set_diagnostic("rflux-errpct", flux_error_percent);
      if (solid_mass_value || reaction_file) {
        model.set_diagnostic("rmflux", measured_flux * solid_mass_per_reaction);
        model.set_diagnostic("rmflux-exact",
                             expected_flux * solid_mass_per_reaction);
      }
      model.set_diagnostic("area-errpct",
                           100.0 * std::abs(surface_area - area_exact) /
                               std::abs(area_exact));
      if (screen) {
        std::fprintf(screen,
                     "IAC surface flux: measured %.8e expected %.8e error %.6g percent\n",
                     measured_flux, expected_flux, flux_error_percent);
      }
      if (logfile) {
        std::fprintf(logfile,
                     "IAC surface flux: measured %.8e expected %.8e error %.6g percent\n",
                     measured_flux, expected_flux, flux_error_percent);
      }
    } catch (const std::exception &ex) {
      root_error = ex.what();
    }
    IACBridge::error_if_root_failed(sparta, root_error);
    return;
  }

  if (std::strcmp(arg[0], "install") == 0) {
    if (narg < 2) {
      error->all(FLERR, "Illegal surface install command");
    }
    int partflag = 1;
    int type = 1;
    const char *particle_flag = value_after(narg - 2, arg + 2, "particle");
    const char *type_value = value_after(narg - 2, arg + 2, "type");
    if (particle_flag) {
      if (std::strcmp(particle_flag, "none") == 0) {
        partflag = 0;
      } else if (std::strcmp(particle_flag, "check") == 0) {
        partflag = 1;
      } else if (std::strcmp(particle_flag, "keep") == 0) {
        partflag = 2;
      } else {
        error->all(FLERR, "surface install particle must be none, check, or keep");
      }
    }
    if (type_value) {
      type = std::atoi(type_value);
    }
    try {
      IACBridge::install_surface(sparta, arg[1], partflag, type);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "write-vtp") == 0) {
    if (narg != 3 && narg < 7) {
      error->all(FLERR, "Illegal surface write-vtp command");
    }
    try {
      std::vector<iac::Model::SurfaceCellField> fields;
      if (narg > 3) {
        if (std::strcmp(arg[3], "fix") != 0) {
          error->all(FLERR, "surface write-vtp optional data must use fix <id>");
        }
        const int ifix = modify->find_fix(arg[4]);
        if (ifix < 0) {
          error->all(FLERR, "surface write-vtp fix ID does not exist");
        }
        Fix *ave = modify->fix[ifix];
        if (!ave->per_surf_flag) {
          error->all(FLERR, "surface write-vtp fix must provide per-surf data");
        }
        if (ave->size_per_surf_cols == 0 && !ave->vector_surf) {
          error->all(FLERR, "surface write-vtp fix vector data is not available");
        }
        if (ave->size_per_surf_cols > 0 && !ave->array_surf) {
          error->all(FLERR, "surface write-vtp fix array data is not available");
        }
        if (std::strcmp(arg[5], "fields") != 0) {
          error->all(FLERR, "surface write-vtp fix data requires fields <name...>");
        }

        const int ncols = ave->size_per_surf_cols == 0 ? 1 : ave->size_per_surf_cols;
        const int nfields = narg - 6;
        if (nfields != ncols) {
          error->all(FLERR, "surface write-vtp field count must match fix per-surf columns");
        }

        fields.resize(static_cast<std::size_t>(nfields));
        for (int j = 0; j < nfields; ++j) {
          fields[static_cast<std::size_t>(j)].name = arg[6 + j];
          fields[static_cast<std::size_t>(j)].values.assign(
              static_cast<std::size_t>(surf->nsurf), 0.0);
        }
        for (int i = 0; i < surf->nown; ++i) {
          const int triangle_id = surf->tris[i].id;
          if (triangle_id <= 0 || triangle_id > surf->nsurf) {
            error->all(FLERR, "surface write-vtp encountered invalid surface ID");
          }
          const std::size_t field_index = static_cast<std::size_t>(triangle_id - 1);
          if (ave->size_per_surf_cols == 0) {
            fields[0].values[field_index] = ave->vector_surf[i];
          } else {
            for (int j = 0; j < nfields; ++j) {
              fields[static_cast<std::size_t>(j)].values[field_index] = ave->array_surf[i][j];
            }
          }
        }
        reduce_surface_fields(sparta, surf, fields);
      }
      if (IACBridge::owns_model(sparta)) {
        IACBridge::model(sparta).write_surface_vtp(arg[1], arg[2], fields);
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "dump") == 0) {
    auto &cfg = IACBridge::config();
    if (narg == 2 && std::strcmp(arg[1], "off") == 0) {
      cfg.surface_dumps.clear();
      if (IACBridge::owns_model(sparta) && IACBridge::has_model()) {
        IACBridge::model(sparta).set_surface_dumps(cfg.surface_dumps);
      }
      return;
    }
    if (narg != 6) {
      error->all(FLERR, "Expected surface dump <id> <surface-id> vtp <N> <path>");
    }
    iac::SurfaceDump dump;
    dump.id = arg[1];
    dump.surface = arg[2];
    dump.style = arg[3];
    dump.every = std::atoi(arg[4]);
    dump.path = arg[5];
    if (dump.style != "vtp") {
      error->all(FLERR, "surface dump style must be vtp");
    }
    if (dump.every <= 0) {
      error->all(FLERR, "surface dump interval must be positive");
    }
    cfg.surface_dumps.push_back(std::move(dump));
    if (IACBridge::owns_model(sparta) && IACBridge::has_model()) {
      IACBridge::model(sparta).set_surface_dumps(cfg.surface_dumps);
    }
    return;
  }

  error->all(FLERR, "Illegal surface command");
}
