#include "surface.h"

#include "error.h"
#include "iacbridge.h"
#include "comm.h"
#include "fix.h"
#include "modify.h"
#include "surf.h"
#include "update.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
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

const char *value_after(int narg, char **arg, const char *key) {
  for (int i = 0; i + 1 < narg; ++i) {
    if (std::strcmp(arg[i], key) == 0) {
      return arg[i + 1];
    }
  }
  return nullptr;
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
  if (comm->nprocs != 1) {
    error->all(FLERR, (std::string(command) + " currently supports one MPI rank").c_str());
  }
  (void)sparta;
  return ave;
}

double fix_surface_value(Fix *ave, int local_index, int column) {
  return ave->size_per_surf_cols == 0 ? ave->vector_surf[local_index]
                                      : ave->array_surf[local_index][column - 1];
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

    if (std::strcmp(arg[2], "dsmc/reaction") == 0) {
      char **pairs = arg + 3;
      const int npairs = narg - 3;
      const char *fix_id = value_after(npairs, pairs, "fix");
      const char *column_value = value_after(npairs, pairs, "column");
      const char *sample_steps_value = value_after(npairs, pairs, "sample-steps");
      const char *solid_mass = value_after(npairs, pairs, "solid-mass-per-reaction");
      const char *reaction_file = value_after(npairs, pairs, "reaction");
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
      if (comm->nprocs != 1) {
        error->all(FLERR, "surface flux dsmc/reaction currently supports one MPI rank");
      }

      const int sample_steps = std::atoi(sample_steps_value);
      const char *ablation_dt_value = value_after(npairs, pairs, "ablation-dt");
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

      try {
        if (ablation_dt_value) {
          IACBridge::model(sparta).set_timestep(ablation_dt);
        } else {
          IACBridge::model(sparta).set_timestep(static_cast<double>(sample_steps) *
                                                update->dt * time_scale);
          IACBridge::set_last_coupling_step(sparta);
        }
        const double dt = IACBridge::model(sparta).timestep();
        const auto triangles = IACBridge::model(sparta).surface_triangles(flux.surface);
        if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
          error->all(FLERR, "surface flux dsmc/reaction triangle count does not match SPARTA surface count");
        }
        std::vector<double> mass_fluxes(static_cast<std::size_t>(surf->nsurf), 0.0);
        for (int i = 0; i < surf->nown; ++i) {
          const int triangle_id = surf->tris[i].id;
          if (triangle_id <= 0 || triangle_id > static_cast<int>(mass_fluxes.size())) {
            error->all(FLERR, "surface flux dsmc/reaction encountered invalid surface ID");
          }
          const double reaction_count =
              ave->size_per_surf_cols == 0 ? ave->vector_surf[i] : ave->array_surf[i][column - 1];
          const std::size_t idx = static_cast<std::size_t>(triangle_id - 1);
          const double area = triangles[idx].area;
          if (area > 0.0) {
            const double mass = reaction_count * static_cast<double>(sample_steps) *
                                update->fnum * solid_mass_per_reaction * time_scale;
            mass_fluxes[idx] = mass / (area * dt);
          }
        }
        IACBridge::model(sparta).apply_triangle_fluxes(flux.surface, mass_fluxes);
      } catch (const std::exception &ex) {
        error->all(FLERR, ex.what());
      }
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
      if (comm->nprocs != 1) {
        error->all(FLERR, "surface flux dsmc/surf currently supports one MPI rank");
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

      std::vector<double> mass_fluxes(static_cast<std::size_t>(surf->nsurf), 0.0);
      for (int i = 0; i < surf->nown; ++i) {
        const int triangle_id = surf->tris[i].id;
        if (triangle_id <= 0 || triangle_id > static_cast<int>(mass_fluxes.size())) {
          error->all(FLERR, "surface flux dsmc/surf encountered invalid surface ID");
        }
        const double number_flux =
            ave->size_per_surf_cols == 0 ? ave->vector_surf[i] : ave->array_surf[i][column - 1];
        mass_fluxes[static_cast<std::size_t>(triangle_id - 1)] =
            number_flux * reaction_prob * solid_mass_per_hit;
      }

      try {
        if (mass_courant_value) {
          IACBridge::model(sparta).set_timestep_from_triangle_fluxes(
              flux.surface, mass_fluxes, mass_courant);
        } else if (ablation_dt_value) {
          IACBridge::model(sparta).set_timestep(ablation_dt);
        } else {
          IACBridge::set_coupling_interval_from_dsmc(sparta);
        }
        IACBridge::model(sparta).apply_triangle_fluxes(flux.surface, mass_fluxes);
      } catch (const std::exception &ex) {
        error->all(FLERR, ex.what());
      }
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
      IACBridge::model(sparta).apply_flux(flux);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "measure-flux") == 0) {
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

    try {
      auto &model = IACBridge::model(sparta);
      const auto triangles = model.surface_triangles(surface_id);
      if (triangles.size() != static_cast<std::size_t>(surf->nsurf)) {
        error->all(FLERR, "surface measure-flux triangle count does not match SPARTA surface count");
      }
      double surface_area = 0.0;
      for (const auto &triangle : triangles) {
        surface_area += triangle.area;
      }
      double reaction_count_sum = 0.0;
      for (int i = 0; i < surf->nown; ++i) {
        reaction_count_sum += fix_surface_value(ave, i, column);
      }
      const double measured_flux = reaction_count_sum * update->fnum /
                                   (surface_area * update->dt);
      const double pi = std::acos(-1.0);
      const double expected_flux =
          mole_fraction * number_density *
          std::sqrt(1.380649e-23 * temperature / (2.0 * pi * molecular_mass)) *
          reaction_prob;
      const auto &cfg = IACBridge::config();
      const double area_exact = cfg.geometry == iac::GeometryKind::Sphere
                                    ? 4.0 * pi * std::pow(0.5 * cfg.sphere.diameter, 2)
                                    : surface_area;
      model.set_diagnostic("surface-area", surface_area);
      model.set_diagnostic("expected-surface-area", area_exact);
      model.set_diagnostic("reaction-count-per-step", reaction_count_sum);
      model.set_diagnostic("expected-reaction-count-per-step",
                           expected_flux * surface_area * update->dt / update->fnum);
      model.set_diagnostic("sample-steps", static_cast<double>(sample_steps));
      model.set_diagnostic("reaction-flux", measured_flux);
      model.set_diagnostic("expected-reaction-flux", expected_flux);
      model.set_diagnostic("reaction-flux-ratio", measured_flux / expected_flux);
      model.set_diagnostic("reaction-flux-error-percent",
                           100.0 * std::abs(measured_flux - expected_flux) /
                               std::abs(expected_flux));
      if (solid_mass_value || reaction_file) {
        model.set_diagnostic("reaction-mass-flux", measured_flux * solid_mass_per_reaction);
        model.set_diagnostic("expected-reaction-mass-flux",
                             expected_flux * solid_mass_per_reaction);
      }
      model.set_diagnostic("surface-area-error-percent",
                           100.0 * std::abs(surface_area - area_exact) /
                               std::abs(area_exact));
      if (screen) {
        std::fprintf(screen,
                     "IAC surface flux: measured %.8e expected %.8e error %.6g percent\n",
                     measured_flux, expected_flux,
                     model.diagnostic("reaction-flux-error-percent"));
      }
      if (logfile) {
        std::fprintf(logfile,
                     "IAC surface flux: measured %.8e expected %.8e error %.6g percent\n",
                     measured_flux, expected_flux,
                     model.diagnostic("reaction-flux-error-percent"));
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
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
        if (comm->nprocs != 1) {
          error->all(FLERR, "surface write-vtp fix fields currently support one MPI rank");
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
      }
      IACBridge::model(sparta).write_surface_vtp(arg[1], arg[2], fields);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "dump") == 0) {
    auto &cfg = IACBridge::config();
    if (narg == 2 && std::strcmp(arg[1], "off") == 0) {
      cfg.surface_dumps.clear();
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
    IACBridge::reset_model();
    return;
  }

  error->all(FLERR, "Illegal surface command");
}
