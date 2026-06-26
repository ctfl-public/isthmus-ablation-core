#include "isthmus_ablation/model.hpp"

#include "isthmus_ablation/expression.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>

#ifdef IAC_HAS_ISTHMUS
#include <isthmus/geometry.hpp>
#include <isthmus/marching_windows.hpp>
#include <isthmus/utilities.hpp>
#endif

namespace iac {
namespace {

constexpr double kEpsilon = 1.0e-12;
constexpr double kBoltzmann = 1.380649e-23;

std::string format_error(const std::string &quantity, double error, double tolerance,
                         const std::string &mode) {
  std::ostringstream out;
  out << "verification failed for '" << quantity << "': error " << std::setprecision(17)
      << error << " exceeds tolerance " << tolerance;
  if (mode == "percent") {
    out << " percent";
  }
  return out.str();
}

const std::vector<std::string> &default_stats_columns() {
  static const std::vector<std::string> columns{
      "step", "time", "nvox", "ndel", "mass", "mf", "vf", "front", "rad"};
  return columns;
}

bool env_flag_enabled(const char *name) {
  const char *value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return false;
  }
  const std::string text(value);
  return text != "0" && text != "false" && text != "off" && text != "no";
}

std::string canonical_quantity(const std::string &quantity) {
  static const std::unordered_map<std::string, std::string> aliases{
      {"nvox", "nvox"},
      {"ndel", "ndel"},
      {"mass", "mass"},
      {"mf", "mf"},
      {"vf", "vf"},
      {"rad", "rad"},
      {"mass0", "mass0"},
      {"rad0", "rad0"},
      {"mvox", "mvox"},
      {"nvox0", "nvox0"},
      {"mreq", "mreq"},
      {"mapp", "mapp"},
      {"mdrop", "mdrop"},
      {"area", "area"},
      {"area-exact", "area-exact"},
      {"area-errpct", "area-errpct"},
      {"nreact", "nreact"},
      {"nreact-exact", "nreact-exact"},
      {"rflux", "rflux"},
      {"rflux-exact", "rflux-exact"},
      {"rflux-ratio", "rflux-ratio"},
      {"rflux-errpct", "rflux-errpct"},
      {"rmflux", "rmflux"},
      {"rmflux-exact", "rmflux-exact"},
  };
  const auto found = aliases.find(quantity);
  return found == aliases.end() ? quantity : found->second;
}

double history_value(const HistoryRow &row, const std::string &column) {
  const std::string quantity = canonical_quantity(column);
  if (quantity == "step") {
    return static_cast<double>(row.step);
  }
  if (quantity == "time") {
    return row.time;
  }
  if (quantity == "nvox") {
    return static_cast<double>(row.active_voxels);
  }
  if (quantity == "ndel") {
    return static_cast<double>(row.deleted_voxels);
  }
  if (quantity == "mass") {
    return row.remaining_mass;
  }
  if (quantity == "mf") {
    return row.mass_fraction;
  }
  if (quantity == "vf") {
    return row.volume_fraction;
  }
  if (quantity == "front") {
    return row.front;
  }
  if (quantity == "rad") {
    return row.radius;
  }
  if (quantity == "mreq") {
    return row.requested_mass_step;
  }
  if (quantity == "mapp") {
    return row.applied_mass_step;
  }
  if (quantity == "mdrop") {
    return row.dropped_mass_step;
  }
  throw RuntimeError("unknown stats_style column '" + column + "'");
}

int stats_column_width(const std::string &column) {
  return std::max(12, static_cast<int>(column.size())) + 1;
}

std::string normalize_quantity(std::string quantity) {
  return canonical_quantity(quantity);
}

double verification_error(double actual, double exact, const VerificationCheck &check) {
  const double absolute_error = std::abs(actual - exact);
  if (check.tolerance_mode == "absolute") {
    return absolute_error;
  }
  if (check.tolerance_mode == "percent") {
    const double denominator = std::abs(exact);
    if (denominator <= kEpsilon) {
      return absolute_error <= kEpsilon ? 0.0 : std::numeric_limits<double>::infinity();
    }
    return 100.0 * absolute_error / denominator;
  }
  throw RuntimeError("unknown verify tolerance mode '" + check.tolerance_mode + "'");
}

double kinetic_theory_mass_flux(const SurfaceFluxCommand &flux) {
  const double pi = std::acos(-1.0);
  const double number_density = flux.mole_fraction * flux.pressure / (kBoltzmann * flux.temperature);
  const double impingement =
      number_density * std::sqrt(kBoltzmann * flux.temperature /
                                 (2.0 * pi * flux.molecular_mass));
  return flux.reaction_prob * flux.solid_mass_per_hit * impingement;
}

bool is_slab_face(const std::string &face) {
  return face == "xlo" || face == "xhi" || face == "ylo" || face == "yhi" ||
         face == "zlo" || face == "zhi";
}

std::string csv_escape(const std::string &value) {
  if (value.find_first_of(",\"\n") == std::string::npos) {
    return value;
  }
  std::string escaped = "\"";
  for (const char c : value) {
    if (c == '"') {
      escaped += "\"\"";
    } else {
      escaped += c;
    }
  }
  escaped += '"';
  return escaped;
}

std::string dump_path_for_step(const std::string &pattern, int step) {
  std::ostringstream step_text;
  step_text << std::setw(6) << std::setfill('0') << step;

  const auto star = pattern.find('*');
  if (star != std::string::npos) {
    std::string path = pattern;
    path.replace(star, 1, step_text.str());
    return path;
  }

  const std::filesystem::path original(pattern);
  const auto parent = original.parent_path();
  const auto stem = original.stem().string();
  const auto extension = original.extension().string();
  const auto filename = stem + "_" + step_text.str() + extension;
  return parent.empty() ? filename : (parent / filename).string();
}

void ensure_parent_directory(const std::string &path) {
  const std::filesystem::path output_path(path);
  if (output_path.has_parent_path()) {
    std::filesystem::create_directories(output_path.parent_path());
  }
}

std::array<double, 3> subtract3(const std::array<double, 3> &a,
                                const std::array<double, 3> &b) {
  return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}

std::array<double, 3> cross3(const std::array<double, 3> &a,
                             const std::array<double, 3> &b) {
  return {{a[1] * b[2] - a[2] * b[1],
           a[2] * b[0] - a[0] * b[2],
           a[0] * b[1] - a[1] * b[0]}};
}

double dot3(const std::array<double, 3> &a, const std::array<double, 3> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double norm3(const std::array<double, 3> &a) {
  return std::sqrt(dot3(a, a));
}

std::array<double, 3> centroid3(const std::array<double, 3> &a,
                                const std::array<double, 3> &b,
                                const std::array<double, 3> &c) {
  return {{(a[0] + b[0] + c[0]) / 3.0, (a[1] + b[1] + c[1]) / 3.0,
           (a[2] + b[2] + c[2]) / 3.0}};
}

std::array<double, 3> normalized3(const std::array<double, 3> &a) {
  const double n = norm3(a);
  if (n <= 0.0) {
    return {{0.0, 0.0, 0.0}};
  }
  return {{a[0] / n, a[1] / n, a[2] / n}};
}

double triangle_area3(const std::array<double, 3> &a, const std::array<double, 3> &b,
                      const std::array<double, 3> &c) {
  return 0.5 * norm3(cross3(subtract3(b, a), subtract3(c, a)));
}

} // namespace

Model::Model(Config config) : config_(std::move(config)) {
  validate_and_initialize();
}

std::vector<Model::PublicSurfaceTriangle> Model::surface_triangles(const std::string &name) const {
  const auto found = surfaces_.find(name);
  if (found == surfaces_.end()) {
    throw RuntimeError("unknown surface '" + name + "'");
  }

  std::vector<PublicSurfaceTriangle> result;
  result.reserve(found->second.triangles.size());
  for (const auto &triangle : found->second.triangles) {
    result.push_back(PublicSurfaceTriangle{triangle.a, triangle.b, triangle.c, triangle.normal,
                                           triangle.area, triangle.last_requested_mass});
  }
  return result;
}

void Model::set_timestep(double dt) {
  if (dt <= 0.0) {
    throw RuntimeError("set_timestep requires a positive timestep");
  }
  dt_ = dt;
}

void Model::set_timestep_from_source_courant(double courant, const std::string &source) {
  if (source != config_.source.name) {
    throw RuntimeError("mass/courant timestep references unknown source '" + source + "'");
  }
  if (courant <= 0.0) {
    throw RuntimeError("mass/courant timestep requires a positive value");
  }
  if (config_.source.value <= 0.0) {
    throw RuntimeError("mass/courant timestep requires a positive source");
  }
  set_timestep(courant * config_.material.density * voxel_dx() / config_.source.value);
}

void Model::reset_run_state() {
  history_.clear();
  current_step_ = 0;
  current_time_ = 0.0;
  step_open_ = false;
  applied_mass_total_ = 0.0;
  record_history(0, 0.0);
  write_scheduled_dumps(0);
}

void Model::generate_surface(const IsthmusSurfaceCommand &surface) {
  validate_isthmus_surface(surface);
  generate_isthmus_surface(surface);
}

void Model::apply_flux(const SurfaceFluxCommand &flux) {
  validate_surface_flux(flux);
  open_step();
  apply_surface_flux(flux);
}

void Model::apply_triangle_fluxes(const std::string &surface_name,
                                  const std::vector<double> &mass_fluxes) {
  auto it = surfaces_.find(surface_name);
  if (it == surfaces_.end()) {
    throw RuntimeError("surface flux references unknown surface '" + surface_name + "'");
  }

  open_step();
  auto &surface = it->second;
  for (auto &triangle : surface.triangles) {
    triangle.requested_mass = 0.0;
    triangle.last_requested_mass = 0.0;
  }
  if (mass_fluxes.size() != surface.triangles.size()) {
    throw RuntimeError("surface flux vector size does not match surface triangle count");
  }
  for (std::size_t i = 0; i < surface.triangles.size(); ++i) {
    const double mass_flux = mass_fluxes[i];
    if (mass_flux <= 0.0) {
      continue;
    }
    auto &triangle = surface.triangles[i];
    triangle.requested_mass = mass_flux * triangle.area * dt_;
    triangle.last_requested_mass = triangle.requested_mass;
    requested_mass_step_ += triangle.requested_mass;
  }
}

void Model::set_timestep_from_triangle_fluxes(const std::string &surface_name,
                                              const std::vector<double> &mass_fluxes,
                                              double courant) {
  if (courant <= 0.0) {
    throw RuntimeError("surface flux mass/courant requires a positive value");
  }
  auto it = surfaces_.find(surface_name);
  if (it == surfaces_.end()) {
    throw RuntimeError("surface flux references unknown surface '" + surface_name + "'");
  }
  const auto &surface = it->second;
  if (mass_fluxes.size() != surface.triangles.size()) {
    throw RuntimeError("surface flux vector size does not match surface triangle count");
  }

  std::vector<double> voxel_mass_rates(voxels_.size(), 0.0);
  std::size_t positive_flux_triangles = 0;
  std::size_t positive_flux_with_owners = 0;
  std::size_t positive_flux_with_active_owners = 0;
  double positive_flux_sum = 0.0;
  double max_positive_flux = 0.0;
  for (std::size_t i = 0; i < surface.triangles.size(); ++i) {
    const double mass_flux = mass_fluxes[i];
    if (mass_flux <= 0.0) {
      continue;
    }
    ++positive_flux_triangles;
    positive_flux_sum += mass_flux;
    max_positive_flux = std::max(max_positive_flux, mass_flux);
    const auto &triangle = surface.triangles[i];
    if (triangle.area <= 0.0 || triangle.voxel_ids.empty()) {
      continue;
    }
    ++positive_flux_with_owners;
    bool has_active_owner = false;
    const double mass_rate = mass_flux * triangle.area;
    for (std::size_t j = 0; j < triangle.voxel_ids.size() && j < triangle.fractions.size(); ++j) {
      const std::size_t voxel_id = triangle.voxel_ids[j];
      const double fraction = triangle.fractions[j];
      if (voxel_id >= voxels_.size() || fraction <= 0.0) {
        continue;
      }
      const auto &voxel = voxels_[voxel_id];
      if (voxel.active && !voxel.fixed && voxel.remaining_mass > 0.0) {
        has_active_owner = true;
        voxel_mass_rates[voxel_id] += mass_rate * fraction;
      }
    }
    if (has_active_owner) {
      ++positive_flux_with_active_owners;
    }
  }
  double max_voxel_mass_rate = 0.0;
  for (const double rate : voxel_mass_rates) {
    max_voxel_mass_rate = std::max(max_voxel_mass_rate, rate);
  }
  if (max_voxel_mass_rate <= 0.0) {
    std::ostringstream message;
    message << "surface flux mass/courant found no positive mapped flux"
            << " (positive-triangles=" << positive_flux_triangles
            << ", positive-with-owners=" << positive_flux_with_owners
            << ", positive-with-active-owners=" << positive_flux_with_active_owners
            << ", positive-flux-sum=" << std::setprecision(8) << positive_flux_sum
            << ", max-positive-flux=" << max_positive_flux << ")";
    throw RuntimeError(message.str());
  }

  const double dx = voxel_dx();
  const double equivalent_face_flux = max_voxel_mass_rate / (dx * dx);
  const double dt = courant * voxel_mass_ / max_voxel_mass_rate;
  set_diagnostic("surface-mass-courant", courant);
  set_diagnostic("surface-max-face-flux", equivalent_face_flux);
  set_diagnostic("surface-max-mvox-rate", max_voxel_mass_rate);
  set_diagnostic("surface-mass-courant-dt", dt);
  set_timestep(dt);
}

void Model::ablate(const AblationCommand &ablate) {
  validate_ablation(ablate, "voxel ablate");
  open_step();
  if (!ablate.surface.empty()) {
    advance_surface_ablation(ablate);
  } else {
    advance_local_slab(ablate);
  }
}

void Model::advance_steps(int steps) {
  advance_steps(steps, nullptr);
}

void Model::advance_steps(int steps, std::ostream *stats) {
  if (steps <= 0) {
    throw RuntimeError("advance_steps requires a positive step count");
  }
  run_steps(RunConfig{false, 0.0, steps}, stats);
}

void Model::validate_and_initialize() {
  if (config_.units != "si") {
    throw RuntimeError("only 'units si' is currently supported");
  }
  if (config_.voxel_name.empty()) {
    throw RuntimeError("missing voxel model; expected 'voxel_create <name> ...'");
  }
  if (config_.material.name.empty() || config_.material.density <= 0.0) {
    throw RuntimeError("voxel material requires a positive density");
  }
  std::string geometry_material;
  if (config_.geometry == GeometryKind::Slab) {
    geometry_material = config_.slab.material;
  } else if (config_.geometry == GeometryKind::Sphere) {
    geometry_material = config_.sphere.material;
  } else if (config_.geometry == GeometryKind::Tiff) {
    geometry_material = config_.tiff.material;
  }
  if (geometry_material != config_.material.name) {
    throw RuntimeError("voxel create references unknown material '" + geometry_material + "'");
  }
  if (config_.source.name.empty() || config_.source.value < 0.0) {
    throw RuntimeError("source constant requires a nonnegative value");
  }
  bool has_run = false;
  bool has_ablation = false;
  for (const auto &command : config_.program) {
    if (command.kind == CommandKind::Run) {
      has_run = true;
      if (!command.run.use_duration && command.run.steps <= 0) {
        throw RuntimeError("run command must specify a positive step count");
      }
      if (command.run.use_duration && command.run.duration <= 0.0) {
        throw RuntimeError("run duration must be positive");
      }
    } else if (command.kind == CommandKind::VoxelAblate) {
      has_ablation = true;
      validate_ablation(command.ablate, "voxel ablate");
    } else if (command.kind == CommandKind::IsthmusSurface) {
      validate_isthmus_surface(command.isthmus_surface);
    } else if (command.kind == CommandKind::SurfaceFlux) {
      validate_surface_flux(command.surface_flux);
    }
  }
  validate_ghosts();
  if (config_.require_program && !has_run) {
    throw RuntimeError("input must contain at least one iac_run command");
  }
  if (config_.require_program && !has_ablation) {
    throw RuntimeError("input must contain a voxel ablate command");
  }

  if (config_.geometry == GeometryKind::Slab) {
    voxel_mass_ = config_.material.density * std::pow(config_.slab.dx, 3);
    initialize_slab();
  } else if (config_.geometry == GeometryKind::Sphere) {
    if (config_.sphere.dx <= 0.0 && config_.sphere.resolution > 0) {
      config_.sphere.dx = config_.sphere.diameter / static_cast<double>(config_.sphere.resolution);
    }
    voxel_mass_ = config_.material.density * std::pow(config_.sphere.dx, 3);
    initialize_sphere();
  } else if (config_.geometry == GeometryKind::Tiff) {
    voxel_mass_ = config_.material.density * std::pow(config_.tiff.dx, 3);
    initialize_tiff();
  } else {
    throw RuntimeError("voxel create requires a supported geometry");
  }
  derive_timestep();
}

void Model::initialize_slab() {
  const auto &g = config_.slab;
  if (g.nx <= 0 || g.ny <= 0 || g.nz <= 0 || g.dx <= 0.0) {
    throw RuntimeError("voxel create slab requires positive nx, ny, nz, and dx");
  }

  voxels_.clear();
  voxels_.reserve(static_cast<std::size_t>(g.nx) * g.ny * g.nz);
  std::size_t id = 0;
  for (int ix = 0; ix < g.nx; ++ix) {
    for (int iy = 0; iy < g.ny; ++iy) {
      for (int iz = 0; iz < g.nz; ++iz) {
        voxels_.push_back(Voxel{id++,
                                ix,
                                iy,
                                iz,
                                (static_cast<double>(ix) + 0.5) * g.dx,
                                (static_cast<double>(iy) + 0.5) * g.dx,
                                (static_cast<double>(iz) + 0.5) * g.dx,
                                voxel_mass_,
                                true,
                                false});
      }
    }
  }

  initial_mass_ = voxel_mass_ * static_cast<double>(voxels_.size());
  initial_active_voxels_ = static_cast<int>(voxels_.size());
}

void Model::initialize_sphere() {
  const auto &g = config_.sphere;
  if (g.diameter <= 0.0 || g.dx <= 0.0) {
    throw RuntimeError("voxel create sphere requires positive diameter and dx or resolution");
  }

  const double radius = 0.5 * g.diameter;
  const int n = static_cast<int>(std::ceil(g.diameter / g.dx));
  if (n <= 0) {
    throw RuntimeError("voxel create sphere generated an empty grid");
  }
  const double origin = -0.5 * static_cast<double>(n) * g.dx;

  voxels_.clear();
  voxels_.reserve(static_cast<std::size_t>(n) * n * n);
  std::size_t id = 0;
  for (int ix = 0; ix < n; ++ix) {
    for (int iy = 0; iy < n; ++iy) {
      for (int iz = 0; iz < n; ++iz) {
        const double x = origin + (static_cast<double>(ix) + 0.5) * g.dx;
        const double y = origin + (static_cast<double>(iy) + 0.5) * g.dx;
        const double z = origin + (static_cast<double>(iz) + 0.5) * g.dx;
        const bool active = x * x + y * y + z * z <= radius * radius;
        voxels_.push_back(Voxel{id++, ix, iy, iz, x, y, z,
                                active ? voxel_mass_ : 0.0, active, false});
      }
    }
  }

  initial_mass_ = remaining_mass();
  initial_active_voxels_ = active_voxel_count();
  if (initial_active_voxels_ <= 0) {
    throw RuntimeError("voxel create sphere produced no active voxels");
  }
}

void Model::initialize_tiff() {
  auto &g = config_.tiff;
  if (g.file.empty() || g.dx <= 0.0) {
    throw RuntimeError("voxel create tiff requires file and positive dx");
  }

  const auto active_voxels =
      isthmus::utilities::load_active_voxels_from_tiff(std::filesystem::path(g.file), g.dx);
  if (active_voxels.voxels.empty()) {
    throw RuntimeError("voxel create tiff produced no active voxels");
  }

  int max_ix = 0;
  int max_iy = 0;
  int max_iz = 0;
  std::set<std::array<int, 3>> active_indices;
  for (const auto &record : active_voxels.voxels) {
    const int ix = static_cast<int>(std::llround(record.centroid[0] / g.dx));
    const int iy = static_cast<int>(std::llround(record.centroid[1] / g.dx));
    const int iz = static_cast<int>(std::llround(record.centroid[2] / g.dx));
    if (ix < 0 || iy < 0 || iz < 0) {
      throw RuntimeError("voxel create tiff received negative voxel coordinates from ISTHMUS");
    }
    max_ix = std::max(max_ix, ix);
    max_iy = std::max(max_iy, iy);
    max_iz = std::max(max_iz, iz);
    active_indices.insert({ix, iy, iz});
  }

  g.nx = max_ix + 1;
  g.ny = max_iy + 1;
  g.nz = max_iz + 1;

  voxels_.clear();
  voxels_.reserve(static_cast<std::size_t>(g.nx) * g.ny * g.nz);
  std::size_t id = 0;
  for (int ix = 0; ix < g.nx; ++ix) {
    for (int iy = 0; iy < g.ny; ++iy) {
      for (int iz = 0; iz < g.nz; ++iz) {
        const bool active = active_indices.count({ix, iy, iz}) != 0;
        voxels_.push_back(Voxel{id++,
                                ix,
                                iy,
                                iz,
                                g.origin[0] + (static_cast<double>(ix) + 0.5) * g.dx,
                                g.origin[1] + (static_cast<double>(iy) + 0.5) * g.dx,
                                g.origin[2] + (static_cast<double>(iz) + 0.5) * g.dx,
                                active ? voxel_mass_ : 0.0,
                                active,
                                false});
      }
    }
  }

  initial_mass_ = remaining_mass();
  initial_active_voxels_ = active_voxel_count();
  if (initial_active_voxels_ <= 0) {
    throw RuntimeError("voxel create tiff produced no active voxels");
  }
}

void Model::derive_timestep() {
  if (config_.timestep.kind == TimestepKind::Explicit) {
    dt_ = config_.timestep.value;
    if (dt_ <= 0.0) {
      throw RuntimeError("explicit timestep must be positive");
    }
    return;
  }

  if (config_.timestep.source != config_.source.name) {
    throw RuntimeError("mass/courant timestep references unknown source '" +
                       config_.timestep.source + "'");
  }
  if (config_.timestep.courant <= 0.0) {
    throw RuntimeError("mass/courant timestep requires a positive value");
  }
  if (config_.source.value <= 0.0) {
    throw RuntimeError("mass/courant timestep requires a positive source");
  }

  const double safe_dt =
      config_.timestep.courant * config_.material.density * voxel_dx() / config_.source.value;
  dt_ = safe_dt;
}

void Model::validate_ablation(const AblationCommand &ablate, const std::string &context) const {
  if (ablate.voxels != config_.voxel_name) {
    throw RuntimeError(context + " references unknown voxel model '" + ablate.voxels + "'");
  }
  if (!ablate.source.empty() && ablate.source != config_.source.name) {
    throw RuntimeError(context + " references unknown source '" + ablate.source + "'");
  }
  if (ablate.source.empty() && ablate.surface.empty()) {
    throw RuntimeError(context + " requires source or surface");
  }
  if (ablate.policy != "local" && ablate.policy != "carryover/normal") {
    throw RuntimeError("only local and carryover/normal ablation policies are currently implemented");
  }
  if (ablate.surface.empty() && ablate.policy != "local") {
    throw RuntimeError(context + " policy '" + ablate.policy + "' requires surface ablation");
  }
  if (ablate.surface.empty() && ablate.face.empty()) {
    throw RuntimeError(context + " source ablation requires face <xlo|xhi|ylo|yhi|zlo|zhi>");
  }
  if (ablate.surface.empty() && !is_slab_face(ablate.face)) {
    throw RuntimeError(context + " face must be one of xlo, xhi, ylo, yhi, zlo, zhi");
  }
  if (!ablate.surface.empty() && !ablate.face.empty()) {
    throw RuntimeError(context + " face is only valid for source ablation; use surface flux selection for surface ablation");
  }
}

void Model::validate_isthmus_surface(const IsthmusSurfaceCommand &surface) const {
  if (surface.voxels != config_.voxel_name) {
    throw RuntimeError("isthmus surface references unknown voxel model '" + surface.voxels + "'");
  }
  if (surface.name.empty()) {
    throw RuntimeError("isthmus surface requires a name");
  }
  if (surface.buffer < 0) {
    throw RuntimeError("isthmus surface buffer must be nonnegative");
  }
  if (surface.resolution <= 0.0) {
    throw RuntimeError("isthmus surface resolution must be positive");
  }
  if (surface.iso_value <= 0.0 || surface.iso_value >= 1.0) {
    throw RuntimeError("isthmus surface iso value must be between 0 and 1");
  }
}

void Model::validate_surface_flux(const SurfaceFluxCommand &flux) const {
  if (flux.style == "source") {
    if (flux.source != config_.source.name) {
      throw RuntimeError("surface flux references unknown source '" + flux.source + "'");
    }
  } else if (flux.style == "kinetic/theory") {
    if (flux.pressure <= 0.0) {
      throw RuntimeError("surface flux kinetic/theory pressure must be positive");
    }
    if (flux.temperature <= 0.0) {
      throw RuntimeError("surface flux kinetic/theory temperature must be positive");
    }
    if (flux.mole_fraction < 0.0 || flux.mole_fraction > 1.0) {
      throw RuntimeError("surface flux kinetic/theory mole-fraction must be between 0 and 1");
    }
    if (flux.molecular_mass <= 0.0) {
      throw RuntimeError("surface flux kinetic/theory molecular-mass must be positive");
    }
    if (flux.reaction_prob < 0.0 || flux.reaction_prob > 1.0) {
      throw RuntimeError("surface flux kinetic/theory reaction-prob must be between 0 and 1");
    }
    if (flux.solid_mass_per_hit <= 0.0) {
      throw RuntimeError("surface flux kinetic/theory solid-mass-per-hit must be positive");
    }
  } else {
    throw RuntimeError("surface flux style must be source or kinetic/theory");
  }
  if (flux.select == "normal" && norm3(flux.direction) <= 0.0) {
    throw RuntimeError("surface flux normal selection requires nonzero direction");
  }
  if (flux.select == "voxels" && flux.voxels != config_.voxel_name) {
    throw RuntimeError("surface flux references unknown voxel model '" + flux.voxels + "'");
  }
  if (flux.select != "all" && flux.select != "normal" && flux.select != "voxels") {
    throw RuntimeError("surface flux select must be all, normal, or voxels");
  }
}

void Model::validate_ghosts() const {
  for (const auto &ghost : config_.ghosts) {
    if (ghost.voxels != config_.voxel_name) {
      throw RuntimeError("voxel ghost references unknown voxel model '" + ghost.voxels + "'");
    }
    if (ghost.axis != "x" && ghost.axis != "y" && ghost.axis != "z") {
      throw RuntimeError("voxel ghost axis must be x, y, or z");
    }
    if (ghost.side != "lo" && ghost.side != "hi" && ghost.side != "both") {
      throw RuntimeError("voxel ghost side must be lo, hi, or both");
    }
    if (ghost.boundary != "infinite") {
      throw RuntimeError("only voxel ghost boundary infinite is supported");
    }
    if (ghost.layers <= 0) {
      throw RuntimeError("voxel ghost layers must be positive");
    }
  }
}

void Model::execute(std::ostream *stats) {
  reset_run_state();
  if (stats != nullptr) {
    print_run_summary(*stats, "Standalone voxel ablation");
    print_header(*stats);
    print_row(*stats, history_.back());
  }

  std::unordered_map<std::string, std::size_t> labels;
  for (std::size_t i = 0; i < config_.program.size(); ++i) {
    if (config_.program[i].kind == CommandKind::Label) {
      labels[config_.program[i].name] = i;
    }
  }

  std::unordered_map<std::string, int> loop_remaining;
  bool skip_next_jump = false;
  std::size_t pc = 0;
  std::size_t executed = 0;
  constexpr std::size_t max_commands = 1000000;
  while (pc < config_.program.size()) {
    if (++executed > max_commands) {
      throw RuntimeError("script exceeded command execution limit; check loop termination");
    }

    const auto &command = config_.program[pc];
    switch (command.kind) {
    case CommandKind::Label:
      ++pc;
      break;
    case CommandKind::VariableLoop:
      loop_remaining[command.name] = command.count;
      ++pc;
      break;
    case CommandKind::Next: {
      const auto it = loop_remaining.find(command.name);
      if (it == loop_remaining.end()) {
        throw RuntimeError("next references unknown loop variable '" + command.name + "'");
      }
      --it->second;
      skip_next_jump = it->second <= 0;
      ++pc;
      break;
    }
    case CommandKind::Jump:
      if (skip_next_jump) {
        skip_next_jump = false;
        ++pc;
      } else if (command.use_time_limit && current_time_ >= command.time_limit - kEpsilon) {
        ++pc;
      } else {
        const auto it = labels.find(command.target);
        if (it == labels.end()) {
          throw RuntimeError("jump references unknown label '" + command.target + "'");
        }
        pc = it->second;
      }
      break;
    case CommandKind::LimitTime: {
      const double remaining = command.time_limit - current_time_;
      if (remaining > kEpsilon && dt_ > remaining) {
        dt_ = remaining;
      }
      ++pc;
      break;
    }
    case CommandKind::Run:
      run_steps(command.run, stats);
      ++pc;
      break;
    case CommandKind::VoxelAblate:
      open_step();
      if (!command.ablate.surface.empty()) {
        advance_surface_ablation(command.ablate);
      } else {
        advance_local_slab(command.ablate);
      }
      ++pc;
      break;
    case CommandKind::IsthmusSurface:
      generate_isthmus_surface(command.isthmus_surface);
      ++pc;
      break;
    case CommandKind::SurfaceFlux:
      open_step();
      apply_surface_flux(command.surface_flux);
      ++pc;
      break;
    }
  }

  for (const auto &dump : config_.dumps) {
    if (dump.style == "history") {
      write_history_csv(dump);
    }
  }
}

void Model::run_steps(const RunConfig &run, std::ostream *stats) {
  const int steps = run.use_duration
                        ? std::max(1, static_cast<int>(std::ceil(run.duration / dt_ - kEpsilon)))
                        : run.steps;
  const double nominal_dt = dt_;
  for (int i = 0; i < steps; ++i) {
    if (run.use_duration) {
      const double remaining = run.duration - current_time_;
      if (remaining <= kEpsilon) {
        break;
      }
      dt_ = std::min(nominal_dt, remaining);
    }
    open_step();
    const int next_step = current_step_ + 1;
    current_step_ = next_step;
    current_time_ += dt_;
    record_history(current_step_, current_time_);
    write_scheduled_dumps(current_step_);
    step_open_ = false;
    if (stats != nullptr && current_step_ % config_.stats.every == 0) {
      print_row(*stats, history_.back());
    }
  }
  if (run.use_duration) {
    dt_ = nominal_dt;
  }
}

void Model::begin_step() {
  requested_mass_step_ = 0.0;
  applied_mass_step_ = 0.0;
  dropped_mass_step_ = 0.0;
}

void Model::open_step() {
  if (!step_open_) {
    begin_step();
    step_open_ = true;
  }
}

void Model::advance_local_slab(const AblationCommand &ablate) {
  const auto &g = config_.slab;
  const double face_area = g.dx * g.dx;
  const auto ablate_voxel = [&](int ix, int iy, int iz) {
    double requested = config_.source.value * face_area * dt_;
    requested_mass_step_ += requested;
    auto &voxel = voxels_[index(ix, iy, iz)];
    const double removed = std::min(voxel.remaining_mass, requested);
    voxel.remaining_mass -= removed;
    applied_mass_step_ += removed;
    applied_mass_total_ += removed;
    requested -= removed;
    if (voxel.remaining_mass <= voxel_mass_ * kEpsilon) {
      voxel.remaining_mass = 0.0;
      if (ablate.delete_empty) {
        voxel.active = false;
      }
    }
    if (requested > 0.0) {
      dropped_mass_step_ += requested;
    }
  };

  if (ablate.face == "xlo" || ablate.face == "xhi") {
    for (int iy = 0; iy < g.ny; ++iy) {
      for (int iz = 0; iz < g.nz; ++iz) {
        for (int offset = 0; offset < g.nx; ++offset) {
          const int ix = ablate.face == "xlo" ? offset : g.nx - 1 - offset;
          if (voxels_[index(ix, iy, iz)].active) {
            ablate_voxel(ix, iy, iz);
            break;
          }
        }
      }
    }
  } else if (ablate.face == "ylo" || ablate.face == "yhi") {
    for (int ix = 0; ix < g.nx; ++ix) {
      for (int iz = 0; iz < g.nz; ++iz) {
        for (int offset = 0; offset < g.ny; ++offset) {
          const int iy = ablate.face == "ylo" ? offset : g.ny - 1 - offset;
          if (voxels_[index(ix, iy, iz)].active) {
            ablate_voxel(ix, iy, iz);
            break;
          }
        }
      }
    }
  } else {
    for (int ix = 0; ix < g.nx; ++ix) {
      for (int iy = 0; iy < g.ny; ++iy) {
        for (int offset = 0; offset < g.nz; ++offset) {
          const int iz = ablate.face == "zlo" ? offset : g.nz - 1 - offset;
          if (voxels_[index(ix, iy, iz)].active) {
            ablate_voxel(ix, iy, iz);
            break;
          }
        }
      }
    }
  }
}

void Model::generate_isthmus_surface(const IsthmusSurfaceCommand &surface) {
#ifndef IAC_HAS_ISTHMUS
  (void)surface;
  throw RuntimeError("isthmus surface requires building with ISTHMUS C++ support");
#else
  const double dx = voxel_dx();
  const double marching_dx = dx * surface.resolution;

  const auto records = surface_voxel_records();
  if (records.empty()) {
    surfaces_[surface.name] = SurfaceState{surface.name, {}};
    return;
  }

#ifndef IAC_ISTHMUS_RUN_OPTIONS_DOMAIN
  std::array<double, 3> lo{{records.front().x, records.front().y, records.front().z}};
  std::array<double, 3> hi = lo;
  for (const auto &record : records) {
    lo[0] = std::min(lo[0], record.x);
    lo[1] = std::min(lo[1], record.y);
    lo[2] = std::min(lo[2], record.z);
    hi[0] = std::max(hi[0], record.x);
    hi[1] = std::max(hi[1], record.y);
    hi[2] = std::max(hi[2], record.z);
  }

  isthmus::DomainConfig domain;
  domain.dimension = isthmus::Dimension::D3;
  domain.voxel_size = dx;
  domain.weighting = surface.weighting;
  domain.iso_value = surface.iso_value;
  for (std::size_t d = 0; d < 3; ++d) {
    domain.limits[0][d] = lo[d] - (static_cast<double>(surface.buffer) + 0.5) * dx;
    domain.limits[1][d] = hi[d] + (static_cast<double>(surface.buffer) + 0.5) * dx;
    domain.cell_counts[d] =
        static_cast<std::size_t>(std::max(1.0, std::ceil((domain.limits[1][d] -
                                                          domain.limits[0][d]) /
                                                         marching_dx - kEpsilon)));
    domain.limits[1][d] = domain.limits[0][d] +
                          static_cast<double>(domain.cell_counts[d]) * marching_dx *
                              (1.0 + 1.0e-10);
  }
#endif
  set_diagnostic("isthmus-surface-resolution", surface.resolution);
  set_diagnostic("isthmus-marching-cell-size", marching_dx);

  isthmus::VoxelSet voxel_set;
  voxel_set.voxels.reserve(records.size());
  for (const auto &surface_voxel : records) {
    isthmus::VoxelRecord record;
    record.centroid = {{surface_voxel.x, surface_voxel.y, surface_voxel.z}};
    record.original_id = surface_voxel.voxel->id;
    record.material_tag = config_.material.name;
    voxel_set.voxels.push_back(std::move(record));
  }

  isthmus::RunOptions options;
#ifdef IAC_ISTHMUS_RUN_OPTIONS_DOMAIN
  options.dimension = isthmus::Dimension::D3;
  options.voxel_size = dx;
  options.marching_voxel_ratio = surface.resolution;
  options.weighting = surface.weighting;
  options.iso_value = surface.iso_value;
#endif
  options.build_surface = true;
  options.build_flux_association = surface.map;
  options.verbose = env_flag_enabled("IAC_ISTHMUS_VERBOSE");

  if (options.verbose) {
    std::cout << "[IAC] generating ISTHMUS surface '" << surface.name << "' from "
              << records.size() << " surface voxels, resolution " << surface.resolution
              << ", iso " << surface.iso_value;
#ifdef IAC_ISTHMUS_RUN_OPTIONS_DOMAIN
    std::cout << ", buffer internal";
#else
    std::cout << ", buffer " << surface.buffer;
#endif
    std::cout << ", map " << (surface.map ? "yes" : "no") << '\n';
  }

  const isthmus::MarchingWindows marching;
#ifdef IAC_ISTHMUS_RUN_OPTIONS_DOMAIN
  const auto result = marching.run(voxel_set, options);
#else
  const auto result = marching.run(domain, voxel_set, options);
#endif
  if (options.verbose) {
    std::cout << "[IAC] generated ISTHMUS surface '" << surface.name << "' with "
              << result.surface_mesh.triangles.size() << " triangles";
    if (surface.map) {
      std::cout << " and " << result.surface_voxels.size() << " mapped surface voxels";
    }
    std::cout << '\n';
  }

  SurfaceState state;
  state.name = surface.name;
  state.triangles.reserve(result.surface_mesh.triangles.size());
  const auto domain_lo = real_domain_lo();
  const auto domain_hi = real_domain_hi();
  for (std::size_t triangle_id = 0; triangle_id < result.surface_mesh.triangles.size();
       ++triangle_id) {
    const auto &connectivity = result.surface_mesh.triangles[triangle_id];
    SurfaceTriangle triangle;
    triangle.a = result.surface_mesh.vertices[connectivity[0]];
    triangle.b = result.surface_mesh.vertices[connectivity[1]];
    triangle.c = result.surface_mesh.vertices[connectivity[2]];
    if (surface.crop_real) {
      const std::array<double, 3> centroid{{
          (triangle.a[0] + triangle.b[0] + triangle.c[0]) / 3.0,
          (triangle.a[1] + triangle.b[1] + triangle.c[1]) / 3.0,
          (triangle.a[2] + triangle.b[2] + triangle.c[2]) / 3.0,
      }};
      const double eps = 1.0e-10 * dx;
      if (centroid[0] < domain_lo[0] - eps || centroid[1] < domain_lo[1] - eps ||
          centroid[2] < domain_lo[2] - eps || centroid[0] > domain_hi[0] + eps ||
          centroid[1] > domain_hi[1] + eps || centroid[2] > domain_hi[2] + eps) {
        continue;
      }
    }
    triangle.area = triangle_area3(triangle.a, triangle.b, triangle.c);
    triangle.normal = normalized3(cross3(subtract3(triangle.b, triangle.a),
                                         subtract3(triangle.c, triangle.a)));
    if (surface.map && triangle_id < result.flux_association.elements.size()) {
      const auto &element = result.flux_association.elements[triangle_id];
      triangle.voxel_ids = element.voxel_ids;
      triangle.fractions = element.scalar_fractions;
    }
    state.triangles.push_back(std::move(triangle));
  }

  surfaces_[surface.name] = std::move(state);
#endif
}

std::vector<Model::SurfaceVoxelRecord> Model::surface_voxel_records() const {
  const double dx = voxel_dx();
  std::vector<SurfaceVoxelRecord> records;
  records.reserve(voxels_.size());
  std::set<std::array<int, 4>> seen;

  const auto add_record = [&](const Voxel &voxel, int ix, int iy, int iz) {
    const std::array<int, 4> key{{ix, iy, iz, static_cast<int>(voxel.id)}};
    if (!seen.insert(key).second) {
      return;
    }
    records.push_back(SurfaceVoxelRecord{&voxel, ix, iy, iz,
                                         voxel.x + static_cast<double>(ix - voxel.ix) * dx,
                                         voxel.y + static_cast<double>(iy - voxel.iy) * dx,
                                         voxel.z + static_cast<double>(iz - voxel.iz) * dx,
                                         ix == voxel.ix && iy == voxel.iy &&
                                             iz == voxel.iz});
  };

  const auto axis_index = [](const std::string &axis) {
    if (axis == "x") {
      return 0;
    }
    if (axis == "y") {
      return 1;
    }
    return 2;
  };

  for (const auto &voxel : voxels_) {
    if (!voxel.active || voxel.remaining_mass <= 0.0) {
      continue;
    }

    add_record(voxel, voxel.ix, voxel.iy, voxel.iz);
    std::vector<std::array<int, 3>> images{{{voxel.ix, voxel.iy, voxel.iz}}};
    for (const auto &ghost : config_.ghosts) {
      const int axis = axis_index(ghost.axis);
      const int count = axis == 0 ? grid_nx() : (axis == 1 ? grid_ny() : grid_nz());
      const std::size_t base_count = images.size();
      for (std::size_t image_id = 0; image_id < base_count; ++image_id) {
        const int value = images[image_id][axis];
        for (int layer = 1; layer <= ghost.layers; ++layer) {
          if ((ghost.side == "lo" || ghost.side == "both") && value == 0) {
            auto image = images[image_id];
            image[axis] = -layer;
            images.push_back(image);
          }
          if ((ghost.side == "hi" || ghost.side == "both") && value + 1 == count) {
            auto image = images[image_id];
            image[axis] = count - 1 + layer;
            images.push_back(image);
          }
        }
      }
    }
    for (std::size_t i = 1; i < images.size(); ++i) {
      add_record(voxel, images[i][0], images[i][1], images[i][2]);
    }
  }

  return records;
}

void Model::apply_surface_flux(const SurfaceFluxCommand &flux) {
  auto it = surfaces_.find(flux.surface);
  if (it == surfaces_.end()) {
    throw RuntimeError("surface flux references unknown surface '" + flux.surface + "'");
  }

  auto &surface = it->second;
  for (auto &triangle : surface.triangles) {
    triangle.requested_mass = 0.0;
    triangle.last_requested_mass = 0.0;
  }

  const double mass_flux =
      flux.style == "kinetic/theory" ? kinetic_theory_mass_flux(flux) : config_.source.value;
  const auto direction = normalized3(flux.direction);
  const auto domain_lo = real_domain_lo();
  const auto domain_hi = real_domain_hi();
  int normal_axis = 0;
  for (int axis = 1; axis < 3; ++axis) {
    if (std::abs(direction[axis]) > std::abs(direction[normal_axis])) {
      normal_axis = axis;
    }
  }
  const double footprint_eps = 1.0e-10 * voxel_dx();
  for (auto &triangle : surface.triangles) {
    bool selected = flux.select == "all";
    if (flux.select == "normal") {
      selected = dot3(triangle.normal, direction) >= flux.min_cos;
      if (selected && !config_.ghosts.empty()) {
        const auto centroid = centroid3(triangle.a, triangle.b, triangle.c);
        for (int axis = 0; axis < 3; ++axis) {
          if (axis == normal_axis) {
            continue;
          }
          if (centroid[axis] < domain_lo[axis] - footprint_eps ||
              centroid[axis] > domain_hi[axis] + footprint_eps) {
            selected = false;
            break;
          }
        }
      }
    } else if (flux.select == "voxels") {
      selected = !triangle.voxel_ids.empty();
    }
    if (!selected) {
      continue;
    }
    triangle.requested_mass = mass_flux * triangle.area * dt_;
    triangle.last_requested_mass = triangle.requested_mass;
    requested_mass_step_ += triangle.requested_mass;
  }
}

void Model::advance_surface_ablation(const AblationCommand &ablate) {
  auto it = surfaces_.find(ablate.surface);
  if (it == surfaces_.end()) {
    throw RuntimeError("voxel ablate references unknown surface '" + ablate.surface + "'");
  }

  std::vector<double> mass_increments(voxels_.size(), 0.0);
  for (auto &triangle : it->second.triangles) {
    if (triangle.requested_mass <= 0.0) {
      continue;
    }
    if (triangle.voxel_ids.empty()) {
      dropped_mass_step_ += triangle.requested_mass;
      triangle.requested_mass = 0.0;
      continue;
    }

    double fraction_sum = 0.0;
    for (const double fraction : triangle.fractions) {
      if (fraction > 0.0) {
        fraction_sum += fraction;
      }
    }
    if (fraction_sum <= 0.0) {
      dropped_mass_step_ += triangle.requested_mass;
      triangle.requested_mass = 0.0;
      continue;
    }

    double mapped = 0.0;
    for (std::size_t i = 0; i < triangle.voxel_ids.size() && i < triangle.fractions.size(); ++i) {
      const auto id = triangle.voxel_ids[i];
      const double fraction = triangle.fractions[i];
      if (fraction <= 0.0) {
        continue;
      }
      const double request = triangle.requested_mass * fraction / fraction_sum;
      mapped += request;
      if (id >= voxels_.size()) {
        dropped_mass_step_ += request;
      } else {
        mass_increments[id] += request;
      }
    }
    if (mapped < triangle.requested_mass) {
      dropped_mass_step_ += triangle.requested_mass - mapped;
    }
    triangle.requested_mass = 0.0;
  }

  if (ablate.policy == "carryover/normal") {
    apply_surface_normal_carryover(mass_increments, ablate);
  } else {
    apply_surface_local_increments(mass_increments, ablate);
  }
}

void Model::apply_surface_local_increments(const std::vector<double> &mass_increments,
                                           const AblationCommand &ablate) {
  for (std::size_t voxel_id = 0; voxel_id < voxels_.size(); ++voxel_id) {
    double requested = voxel_id < mass_increments.size() ? mass_increments[voxel_id] : 0.0;
    if (requested <= 0.0) {
      continue;
    }
    auto &voxel = voxels_[voxel_id];
    if (!voxel.active || voxel.fixed || voxel.remaining_mass <= 0.0) {
      dropped_mass_step_ += requested;
      continue;
    }

    const double removed = std::min(voxel.remaining_mass, requested);
    voxel.remaining_mass -= removed;
    applied_mass_step_ += removed;
    applied_mass_total_ += removed;
    requested -= removed;
    if (voxel.remaining_mass <= voxel_mass_ * kEpsilon) {
      voxel.remaining_mass = 0.0;
      if (ablate.delete_empty) {
        voxel.active = false;
      }
    }
    if (requested > 0.0) {
      dropped_mass_step_ += requested;
    }
  }
}

void Model::apply_surface_normal_carryover(const std::vector<double> &mass_increments,
                                           const AblationCommand &ablate) {
  std::vector<double> removed(voxels_.size(), 0.0);
  std::vector<bool> keep(voxels_.size(), false);
  std::vector<bool> processed(voxels_.size(), false);

  for (std::size_t i = 0; i < voxels_.size(); ++i) {
    const auto &voxel = voxels_[i];
    if (voxel.active && voxel.remaining_mass > 0.0 && !voxel.fixed) {
      keep[i] = true;
      removed[i] = voxel_mass_ - voxel.remaining_mass;
    }
  }

  std::vector<std::size_t> queue;
  queue.reserve(voxels_.size());
  for (std::size_t i = 0; i < voxels_.size(); ++i) {
    const double increment = i < mass_increments.size() ? mass_increments[i] : 0.0;
    if (increment <= 0.0) {
      continue;
    }
    if (!keep[i]) {
      dropped_mass_step_ += increment;
      continue;
    }
    removed[i] += increment;
    if (removed[i] >= voxel_mass_ * (1.0 - kEpsilon)) {
      queue.push_back(i);
    }
  }

  const auto center = carryover_center();
  const int nx = grid_nx();
  const int ny = grid_ny();
  const int nz = grid_nz();
  constexpr int offsets[26][3] = {
      {-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1}, {-1, 0, -1}, {-1, 0, 0},
      {-1, 0, 1},  {-1, 1, -1}, {-1, 1, 0},  {-1, 1, 1},  {0, -1, -1},
      {0, -1, 0},  {0, -1, 1},  {0, 0, -1},  {0, 0, 1},   {0, 1, -1},
      {0, 1, 0},   {0, 1, 1},   {1, -1, -1}, {1, -1, 0},  {1, -1, 1},
      {1, 0, -1},  {1, 0, 0},   {1, 0, 1},   {1, 1, -1},  {1, 1, 0},
      {1, 1, 1},
  };

  while (!queue.empty()) {
    const std::size_t i = queue.back();
    queue.pop_back();
    if (i >= voxels_.size() || processed[i] || !keep[i] ||
        removed[i] < voxel_mass_ * (1.0 - kEpsilon)) {
      continue;
    }
    processed[i] = true;
    const double excess = std::max(removed[i] - voxel_mass_, 0.0);
    removed[i] = voxel_mass_;
    keep[i] = false;
    if (excess <= 0.0) {
      continue;
    }

    const auto &from_voxel = voxels_[i];
    const std::array<double, 3> from{{from_voxel.x, from_voxel.y, from_voxel.z}};
    auto inward = normalized3({{center[0] - from[0], center[1] - from[1], center[2] - from[2]}});
    if (norm3(inward) <= 0.0) {
      dropped_mass_step_ += excess;
      continue;
    }

    std::vector<std::size_t> candidates;
    std::vector<double> weights;
    double weight_sum = 0.0;
    for (const auto &offset : offsets) {
      const int jx = from_voxel.ix + offset[0];
      const int jy = from_voxel.iy + offset[1];
      const int jz = from_voxel.iz + offset[2];
      if (jx < 0 || jy < 0 || jz < 0 || jx >= nx || jy >= ny || jz >= nz) {
        continue;
      }
      const std::size_t j = index(jx, jy, jz);
      if (j >= voxels_.size() || !keep[j]) {
        continue;
      }
      const auto &to_voxel = voxels_[j];
      const std::array<double, 3> to{{to_voxel.x, to_voxel.y, to_voxel.z}};
      auto direction = subtract3(to, from);
      const double direction_norm = norm3(direction);
      if (direction_norm <= 0.0) {
        continue;
      }
      direction[0] /= direction_norm;
      direction[1] /= direction_norm;
      direction[2] /= direction_norm;
      const double projection = dot3(direction, inward);
      if (projection <= 1.0e-12) {
        continue;
      }
      const double weight = projection / direction_norm;
      candidates.push_back(j);
      weights.push_back(weight);
      weight_sum += weight;
    }

    if (candidates.empty() || weight_sum <= 0.0) {
      dropped_mass_step_ += excess;
      continue;
    }
    for (std::size_t k = 0; k < candidates.size(); ++k) {
      const std::size_t j = candidates[k];
      removed[j] += excess * weights[k] / weight_sum;
      if (removed[j] >= voxel_mass_ * (1.0 - kEpsilon) && !processed[j]) {
        queue.push_back(j);
      }
    }
  }

  for (std::size_t i = 0; i < voxels_.size(); ++i) {
    auto &voxel = voxels_[i];
    if (!voxel.active || voxel.fixed) {
      continue;
    }
    const double old_mass = voxel.remaining_mass;
    voxel.remaining_mass = std::max(voxel_mass_ - removed[i], 0.0);
    if (old_mass > voxel.remaining_mass) {
      const double removed_now = old_mass - voxel.remaining_mass;
      applied_mass_step_ += removed_now;
      applied_mass_total_ += removed_now;
    }
    if (!keep[i] || voxel.remaining_mass <= voxel_mass_ * kEpsilon) {
      voxel.remaining_mass = 0.0;
      if (ablate.delete_empty) {
        voxel.active = false;
      }
    }
  }
}

void Model::record_history(int step, double time) {
  history_.push_back(make_history_row(step, time));
}

HistoryRow Model::make_history_row(int step, double time) const {
  const double remaining = remaining_mass();
  const int front = front_ix();
  const double discrete_front =
      config_.geometry == GeometryKind::Sphere
          ? 0.0
          : (front < 0 ? static_cast<double>(grid_nx()) * voxel_dx()
                       : static_cast<double>(front) * voxel_dx());

  HistoryRow row;
  row.step = step;
  row.time = time;
  row.active_voxels = active_voxel_count();
  row.deleted_voxels = deleted_voxel_count();
  row.remaining_mass = remaining;
  row.mass_fraction = initial_mass_ > 0.0 ? remaining / initial_mass_ : 0.0;
  row.volume_fraction = initial_active_voxels_ > 0
                            ? static_cast<double>(row.active_voxels) /
                                  static_cast<double>(initial_active_voxels_)
                            : 0.0;
  row.front = discrete_front;
  row.radius = inferred_radius();
  row.requested_mass_step = requested_mass_step_;
  row.applied_mass_step = applied_mass_step_;
  row.dropped_mass_step = dropped_mass_step_;
  return row;
}

void Model::write_scheduled_dumps(int step) const {
  for (const auto &dump : config_.dumps) {
    if (dump.voxels != config_.voxel_name) {
      throw RuntimeError("voxel dump '" + dump.id + "' references unknown voxel model '" +
                         dump.voxels + "'");
    }
    if (dump.style != "vtu") {
      continue;
    }
    if (step % dump.every == 0) {
      write_vtu(dump, step);
    }
  }
  for (const auto &dump : config_.surface_dumps) {
    if (step % dump.every == 0) {
      write_vtp(dump, step);
    }
  }
}

void Model::write_history_csv(const VoxelDump &dump) const {
  write_history(dump.path);
}

void Model::write_history(const std::string &path) const {
  ensure_parent_directory(path);
  std::ofstream out(path);
  if (!out) {
    throw RuntimeError("could not write history file '" + path + "'");
  }
  out << "step,time,nvox,ndel,mass,mf,vf,front,rad,mreq,mapp,mdrop\n";
  out << std::setprecision(17);
  for (const auto &row : history_) {
    out << row.step << ',' << row.time << ',' << row.active_voxels << ','
        << row.deleted_voxels << ',' << row.remaining_mass << ',' << row.mass_fraction
        << ',' << row.volume_fraction << ',' << row.front << ',' << row.radius << ','
        << row.requested_mass_step << ',' << row.applied_mass_step << ','
        << row.dropped_mass_step << '\n';
  }
}

void Model::write_voxels_vtu(const std::string &path, const std::string &select,
                             const std::string &scalar) const {
  if (select != "all" && select != "active" && select != "ghosted" &&
      select != "ghosts") {
    throw RuntimeError("voxel write-vtu select must be all, active, ghosted, or ghosts");
  }
  const std::string canonical_scalar = canonical_quantity(scalar);
  if (canonical_scalar != "mf" && canonical_scalar != "mass" && scalar != "active" &&
      scalar != "fixed" && scalar != "id" && scalar != "ix" && scalar != "iy" &&
      scalar != "iz" && scalar != "ghost") {
    throw RuntimeError("unknown voxel write-vtu scalar '" + scalar + "'");
  }
  VoxelDump dump;
  dump.id = "write-vtu";
  dump.voxels = config_.voxel_name;
  dump.style = "vtu";
  dump.every = 1;
  dump.path = path;
  dump.select = select;
  dump.scalar = canonical_scalar;
  write_vtu(dump, current_step_);
}

void Model::write_surface_vtp(const std::string &surface, const std::string &path) const {
  write_surface_vtp(surface, path, {});
}

void Model::write_surface_vtp(const std::string &surface, const std::string &path,
                              const std::vector<SurfaceCellField> &fields) const {
  SurfaceDump dump;
  dump.id = "write-vtp";
  dump.surface = surface;
  dump.style = "vtp";
  dump.every = 1;
  dump.path = path;
  write_vtp(dump, current_step_, fields);
}

void Model::write_vtu(const VoxelDump &dump, int step) const {
  const auto path = dump_path_for_step(dump.path, step);
  ensure_parent_directory(path);
  std::ofstream out(path);
  if (!out) {
    throw RuntimeError("could not write VTU file '" + path + "'");
  }

  std::vector<SurfaceVoxelRecord> selected;
  if (dump.select == "ghosted" || dump.select == "ghosts") {
    selected = surface_voxel_records();
    if (dump.select == "ghosts") {
      selected.erase(std::remove_if(selected.begin(), selected.end(),
                                    [](const SurfaceVoxelRecord &record) {
                                      return record.real;
                                    }),
                     selected.end());
    }
  } else {
    selected.reserve(voxels_.size());
    for (const auto &voxel : voxels_) {
      if (dump.select == "active" && !voxel.active) {
        continue;
      }
      selected.push_back(SurfaceVoxelRecord{&voxel,
                                            voxel.ix,
                                            voxel.iy,
                                            voxel.iz,
                                            voxel.x,
                                            voxel.y,
                                            voxel.z,
                                            true});
    }
  }

  const double dx = voxel_dx();
  out << std::setprecision(17);
  out << "<?xml version=\"1.0\"?>\n";
  out << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  out << "  <UnstructuredGrid>\n";
  out << "    <Piece NumberOfPoints=\"" << selected.size() * 8 << "\" NumberOfCells=\""
      << selected.size() << "\">\n";

  out << "      <Points>\n";
  out << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    const double x0 = record.x - 0.5 * dx;
    const double y0 = record.y - 0.5 * dx;
    const double z0 = record.z - 0.5 * dx;
    const double x1 = record.x + 0.5 * dx;
    const double y1 = record.y + 0.5 * dx;
    const double z1 = record.z + 0.5 * dx;
    out << "          " << x0 << ' ' << y0 << ' ' << z0 << ' ' << x1 << ' ' << y0 << ' '
        << z0 << ' ' << x1 << ' ' << y1 << ' ' << z0 << ' ' << x0 << ' ' << y1 << ' '
        << z0 << ' ' << x0 << ' ' << y0 << ' ' << z1 << ' ' << x1 << ' ' << y0 << ' '
        << z1 << ' ' << x1 << ' ' << y1 << ' ' << z1 << ' ' << x0 << ' ' << y1 << ' '
        << z1 << '\n';
  }
  out << "        </DataArray>\n";
  out << "      </Points>\n";

  out << "      <Cells>\n";
  out << "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">\n";
  for (std::size_t i = 0; i < selected.size(); ++i) {
    const std::size_t base = i * 8;
    out << "          " << base << ' ' << base + 1 << ' ' << base + 2 << ' ' << base + 3
        << ' ' << base + 4 << ' ' << base + 5 << ' ' << base + 6 << ' ' << base + 7
        << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">\n";
  for (std::size_t i = 0; i < selected.size(); ++i) {
    out << "          " << (i + 1) * 8 << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n";
  for (std::size_t i = 0; i < selected.size(); ++i) {
    out << "          12\n";
  }
  out << "        </DataArray>\n";
  out << "      </Cells>\n";

  out << "      <CellData Scalars=\"" << dump.scalar << "\">\n";
  out << "        <DataArray type=\"Float64\" Name=\"mf\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    const auto *voxel = record.voxel;
    out << "          " << (voxel_mass_ > 0.0 ? voxel->remaining_mass / voxel_mass_ : 0.0)
        << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Float64\" Name=\"mass\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << record.voxel->remaining_mass << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int64\" Name=\"id\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << record.voxel->id << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"ix\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << record.ix << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"iy\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << record.iy << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"iz\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << record.iz << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"active\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << (record.voxel->active ? 1 : 0) << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"fixed\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << (record.voxel->fixed ? 1 : 0) << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"ghost\" format=\"ascii\">\n";
  for (const auto &record : selected) {
    out << "          " << (record.real ? 0 : 1) << '\n';
  }
  out << "        </DataArray>\n";
  out << "      </CellData>\n";
  out << "    </Piece>\n";
  out << "  </UnstructuredGrid>\n";
  out << "</VTKFile>\n";
}

void Model::write_vtp(const SurfaceDump &dump, int step,
                      const std::vector<SurfaceCellField> &fields) const {
  const auto found = surfaces_.find(dump.surface);
  if (found == surfaces_.end()) {
    return;
  }

  const auto path = dump_path_for_step(dump.path, step);
  ensure_parent_directory(path);
  std::ofstream out(path);
  if (!out) {
    throw RuntimeError("could not write VTP file '" + path + "'");
  }

  const auto &triangles = found->second.triangles;
  for (const auto &field : fields) {
    if (field.values.size() != triangles.size()) {
      throw RuntimeError("surface VTP field '" + field.name +
                         "' does not match triangle count");
    }
  }
  out << std::setprecision(17);
  out << "<?xml version=\"1.0\"?>\n";
  out << "<VTKFile type=\"PolyData\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  out << "  <PolyData>\n";
  out << "    <Piece NumberOfPoints=\"" << triangles.size() * 3
      << "\" NumberOfPolys=\"" << triangles.size() << "\">\n";
  out << "      <Points>\n";
  out << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    out << "          " << triangle.a[0] << ' ' << triangle.a[1] << ' ' << triangle.a[2]
        << ' ' << triangle.b[0] << ' ' << triangle.b[1] << ' ' << triangle.b[2] << ' '
        << triangle.c[0] << ' ' << triangle.c[1] << ' ' << triangle.c[2] << '\n';
  }
  out << "        </DataArray>\n";
  out << "      </Points>\n";
  out << "      <Polys>\n";
  out << "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">\n";
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    const std::size_t base = i * 3;
    out << "          " << base << ' ' << base + 1 << ' ' << base + 2 << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">\n";
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    out << "          " << (i + 1) * 3 << '\n';
  }
  out << "        </DataArray>\n";
  out << "      </Polys>\n";
  out << "      <CellData Scalars=\"area\">\n";
  out << "        <DataArray type=\"Float64\" Name=\"area\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    out << "          " << triangle.area << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Float64\" Name=\"mreq\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    out << "          " << triangle.requested_mass << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Float64\" Name=\"mreq-last\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    out << "          " << triangle.last_requested_mass << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Float64\" Name=\"mflux\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    const double flux = triangle.area > 0.0 && dt_ > 0.0
                            ? triangle.requested_mass / (triangle.area * dt_)
                            : 0.0;
    out << "          " << flux << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Float64\" Name=\"mflux-last\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    const double flux = triangle.area > 0.0 && dt_ > 0.0
                            ? triangle.last_requested_mass / (triangle.area * dt_)
                            : 0.0;
    out << "          " << flux << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"selected\" format=\"ascii\">\n";
  for (const auto &triangle : triangles) {
    out << "          " << (triangle.last_requested_mass > 0.0 ? 1 : 0) << '\n';
  }
  out << "        </DataArray>\n";
  for (const auto &field : fields) {
    out << "        <DataArray type=\"Float64\" Name=\"" << field.name
        << "\" format=\"ascii\">\n";
    for (const double value : field.values) {
      out << "          " << value << '\n';
    }
    out << "        </DataArray>\n";
  }
  out << "      </CellData>\n";
  out << "    </Piece>\n";
  out << "  </PolyData>\n";
  out << "</VTKFile>\n";
}

void Model::write_verification_csv(const std::string &path) const {
  if (history_.empty()) {
    throw RuntimeError("cannot write verification report before run");
  }
  ensure_parent_directory(path);
  std::ofstream out(path);
  if (!out) {
    throw RuntimeError("could not write verification report '" + path + "'");
  }
  out << "quantity,expression,step,time,actual,exact,error,tolerance,tolerance-mode,norm,pass\n";
  out << std::setprecision(17);
  for (const auto &check : config_.checks) {
    const std::string quantity = normalize_quantity(check.quantity);
    for (const auto &row : history_) {
      const double dx = voxel_dx();
      const double length = config_.geometry == GeometryKind::Sphere
                                ? config_.sphere.diameter
                                : static_cast<double>(grid_nx()) * dx;
      const double area = config_.geometry == GeometryKind::Sphere
                              ? 4.0 * std::acos(-1.0) * row.radius * row.radius
                              : static_cast<double>(grid_ny()) * static_cast<double>(grid_nz()) *
                                    dx * dx;
      const double initial_radius =
          config_.geometry == GeometryKind::Sphere ? 0.5 * config_.sphere.diameter : 0.0;
      std::unordered_map<std::string, double> variables{
          {"step", static_cast<double>(row.step)},
          {"time", row.time},
          {"t", row.time},
          {"dt", dt_},
          {"rho", config_.material.density},
          {"density", config_.material.density},
          {"dx", dx},
          {"nx", static_cast<double>(grid_nx())},
          {"ny", static_cast<double>(grid_ny())},
          {"nz", static_cast<double>(grid_nz())},
          {"length", length},
          {"diameter", config_.sphere.diameter},
          {"rad0", initial_radius},
          {"rad", row.radius},
          {"area", area},
          {"mass0", initial_mass_},
          {"mvox", voxel_mass_},
          {"nvox0", static_cast<double>(initial_active_voxels_)},
          {"nvox", static_cast<double>(row.active_voxels)},
          {"ndel", static_cast<double>(row.deleted_voxels)},
          {"mass", row.remaining_mass},
          {"mf", row.mass_fraction},
          {"vf", row.volume_fraction},
          {"front", row.front},
          {"mreq", row.requested_mass_step},
          {"mapp", row.applied_mass_step},
          {"mdrop", row.dropped_mass_step},
          {config_.source.name, config_.source.value},
          {"source:" + config_.source.name, config_.source.value},
      };
      const double actual = history_value(row, quantity);
      const double exact = evaluate_expression(check.expression, variables);
      const double error = ::iac::verification_error(actual, exact, check);
      out << csv_escape(check.quantity) << ',' << csv_escape(check.expression) << ','
          << row.step << ',' << row.time << ',' << actual << ',' << exact << ','
          << error << ',' << check.tolerance << ',' << csv_escape(check.tolerance_mode)
          << ',' << csv_escape(check.norm) << ','
          << (error <= check.tolerance ? "yes" : "no") << '\n';
    }
  }
}

void Model::verify() const {
  if (history_.empty()) {
    throw RuntimeError("cannot verify before run");
  }
  for (const auto &check : config_.checks) {
    const double error = verification_error(check);
    if (!(error <= check.tolerance)) {
      throw RuntimeError(format_error(check.quantity, error, check.tolerance,
                                      check.tolerance_mode));
    }
  }
}

double Model::verification_error(const VerificationCheck &check) const {
  if (history_.empty()) {
    throw RuntimeError("cannot verify before run");
  }
  const std::string quantity = normalize_quantity(check.quantity);
  double accumulated = 0.0;
  double max_error = 0.0;
  int count = 0;
  const auto evaluate_row = [&](const HistoryRow &row) {
    const double dx = voxel_dx();
    const double length = config_.geometry == GeometryKind::Sphere
                              ? config_.sphere.diameter
                              : static_cast<double>(grid_nx()) * dx;
    const double area = config_.geometry == GeometryKind::Sphere
                            ? 4.0 * std::acos(-1.0) * row.radius * row.radius
                            : static_cast<double>(grid_ny()) * static_cast<double>(grid_nz()) *
                                  dx * dx;
    const double initial_radius =
        config_.geometry == GeometryKind::Sphere ? 0.5 * config_.sphere.diameter : 0.0;
    std::unordered_map<std::string, double> variables{
        {"step", static_cast<double>(row.step)},
        {"time", row.time},
        {"t", row.time},
        {"dt", dt_},
        {"rho", config_.material.density},
        {"density", config_.material.density},
        {"dx", dx},
        {"nx", static_cast<double>(grid_nx())},
        {"ny", static_cast<double>(grid_ny())},
        {"nz", static_cast<double>(grid_nz())},
        {"length", length},
        {"diameter", config_.sphere.diameter},
        {"rad0", initial_radius},
        {"rad", row.radius},
        {"area", area},
        {"mass0", initial_mass_},
        {"mvox", voxel_mass_},
        {"nvox0", static_cast<double>(initial_active_voxels_)},
        {"nvox", static_cast<double>(row.active_voxels)},
        {"ndel", static_cast<double>(row.deleted_voxels)},
        {"mass", row.remaining_mass},
        {"mf", row.mass_fraction},
        {"vf", row.volume_fraction},
        {"front", row.front},
        {"mreq", row.requested_mass_step},
        {"mapp", row.applied_mass_step},
        {"mdrop", row.dropped_mass_step},
        {config_.source.name, config_.source.value},
        {"source:" + config_.source.name, config_.source.value},
    };
    const double actual = history_value(row, quantity);
    const double exact = evaluate_expression(check.expression, variables);
    return ::iac::verification_error(actual, exact, check);
  };

  if (check.norm == "final") {
    return evaluate_row(history_.back());
  }
  if (check.norm == "max" || check.norm == "rms") {
    for (const auto &row : history_) {
      const double error = evaluate_row(row);
      max_error = std::max(max_error, error);
      accumulated += error * error;
      ++count;
    }
    if (check.norm == "rms") {
      return count > 0 ? std::sqrt(accumulated / static_cast<double>(count)) : 0.0;
    }
    return max_error;
  }
  throw RuntimeError("unknown verify norm '" + check.norm + "'");
}

void Model::set_diagnostic(const std::string &name, double value) {
  diagnostics_[name] = value;
  diagnostics_[normalize_quantity(name)] = value;
}

double Model::diagnostic(const std::string &name) const {
  const auto found = diagnostics_.find(name);
  if (found != diagnostics_.end()) {
    return found->second;
  }
  const auto normalized = diagnostics_.find(normalize_quantity(name));
  if (normalized != diagnostics_.end()) {
    return normalized->second;
  }
  throw RuntimeError("unknown diagnostic '" + name + "'");
}

bool Model::has_diagnostic(const std::string &name) const {
  return diagnostics_.find(name) != diagnostics_.end() ||
         diagnostics_.find(normalize_quantity(name)) != diagnostics_.end();
}

double Model::diagnostic_verification_error(const VerificationCheck &check) const {
  std::unordered_map<std::string, double> variables = diagnostics_;
  variables["pi"] = std::acos(-1.0);
  variables["dt"] = dt_;
  variables["time"] = current_time_;
  variables["t"] = current_time_;
  variables["step"] = static_cast<double>(current_step_);
  variables["rho"] = config_.material.density;
  variables["density"] = config_.material.density;
  if (config_.geometry == GeometryKind::Sphere) {
    variables["diameter"] = config_.sphere.diameter;
    variables["rad0"] = 0.5 * config_.sphere.diameter;
  }
  variables["mass0"] = initial_mass_;
  variables["mvox"] = voxel_mass_;
  variables["nvox0"] = static_cast<double>(initial_active_voxels_);
  if (!history_.empty()) {
    const auto &row = history_.back();
    variables["nvox"] = static_cast<double>(row.active_voxels);
    variables["ndel"] = static_cast<double>(row.deleted_voxels);
    variables["mass"] = row.remaining_mass;
    variables["mf"] = row.mass_fraction;
    variables["vf"] = row.volume_fraction;
    variables["front"] = row.front;
    variables["rad"] = row.radius;
  }
  if (!config_.source.name.empty()) {
    variables[config_.source.name] = config_.source.value;
    variables["source:" + config_.source.name] = config_.source.value;
  }

  const double actual = diagnostic(check.quantity);
  const double exact = evaluate_expression(check.expression, variables);
  return ::iac::verification_error(actual, exact, check);
}

void Model::verify_diagnostic(const VerificationCheck &check) const {
  const double error = diagnostic_verification_error(check);
  if (!(error <= check.tolerance)) {
    throw RuntimeError(format_error(check.quantity, error, check.tolerance,
                                    check.tolerance_mode));
  }
}

void Model::print_run_summary_public(std::ostream &out) const {
  print_run_summary(out, "Standalone voxel ablation");
}

void Model::print_run_summary_public(std::ostream &out, const std::string &run_type) const {
  print_run_summary(out, run_type);
}

void Model::print_stats_header(std::ostream &out) const {
  print_header(out);
}

void Model::print_latest_stats(std::ostream &out) const {
  if (history_.empty()) {
    throw RuntimeError("cannot print stats before model initialization");
  }
  print_row(out, history_.back());
}

void Model::set_stats_config(const StatsConfig &stats) {
  config_.stats = stats;
}

void Model::print_header(std::ostream &out) const {
  const auto &columns =
      config_.stats.columns.empty() ? default_stats_columns() : config_.stats.columns;
  out << "#";
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (i != 0) {
      out << ' ';
    }
    int width = stats_column_width(columns[i]);
    if (i == 0) {
      width = std::max(1, width - 1);
    }
    out << std::setw(width) << columns[i];
  }
  out << '\n';
}

void Model::print_row(std::ostream &out, const HistoryRow &row) const {
  const auto &columns =
      config_.stats.columns.empty() ? default_stats_columns() : config_.stats.columns;
  out << std::setprecision(6);
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (i != 0) {
      out << ' ';
    }
    const int width = stats_column_width(columns[i]);
    const double value = history_value(row, columns[i]);
    const std::string quantity = canonical_quantity(columns[i]);
    if (quantity == "step" || quantity == "nvox" || quantity == "ndel") {
      out << std::setw(width) << static_cast<long long>(value);
    } else {
      out << std::setw(width) << std::defaultfloat << value;
    }
  }
  out << '\n';
}

void Model::print_run_summary(std::ostream &out, const std::string &run_type) const {
  out << "# " << run_type << '\n';
  out << "# run configuration\n";
  out << "#   voxel model = " << config_.voxel_name << '\n';
  if (config_.geometry == GeometryKind::Slab) {
    const auto &g = config_.slab;
    out << "#   geometry = slab\n";
    out << "#   grid = " << g.nx << " x " << g.ny << " x " << g.nz << '\n';
    out << "#   dx = " << std::setprecision(8) << g.dx << " m\n";
  } else if (config_.geometry == GeometryKind::Sphere) {
    out << "#   geometry = sphere\n";
    out << "#   diameter = " << std::setprecision(8) << config_.sphere.diameter << " m\n";
    if (config_.sphere.resolution > 0) {
      out << "#   resolution = " << config_.sphere.resolution << " voxels across diameter\n";
    }
    out << "#   dx = " << config_.sphere.dx << " m\n";
  } else if (config_.geometry == GeometryKind::Tiff) {
    const auto &g = config_.tiff;
    out << "#   geometry = tiff\n";
    out << "#   file = " << g.file << '\n';
    out << "#   grid = " << g.nx << " x " << g.ny << " x " << g.nz << '\n';
    out << "#   dx = " << std::setprecision(8) << g.dx << " m\n";
  }
  out << "#   material = " << config_.material.name << '\n';
  out << "#   density = " << config_.material.density << " kg/m^3\n";
  out << "#   voxel mass = " << voxel_mass_ << " kg\n";
  out << "#   initial active voxels = " << initial_active_voxels_ << '\n';
  out << "#   initial mass = " << initial_mass_ << " kg\n";
  out << "#   source = " << config_.source.name << " constant " << config_.source.value;
  if (!config_.source.units.empty()) {
    out << " " << config_.source.units;
  }
  out << '\n';
  out << "#   timestep = " << dt_ << " s\n";
  out << "#   program commands = " << config_.program.size() << '\n';
  out << "#   stats = every " << config_.stats.every << " steps\n";
  out << "#   voxel dumps = ";
  if (config_.dumps.empty()) {
    out << "off\n";
  } else {
    out << config_.dumps.size() << '\n';
    for (const auto &dump : config_.dumps) {
      out << "#     " << dump.id << " " << dump.style << " every " << dump.every
          << " path " << dump.path;
      if (dump.style == "vtu") {
        out << " select " << dump.select << " scalar " << dump.scalar;
      }
      out << '\n';
    }
  }
  out << "#   surface dumps = ";
  if (config_.surface_dumps.empty()) {
    out << "off\n";
  } else {
    out << config_.surface_dumps.size() << '\n';
    for (const auto &dump : config_.surface_dumps) {
      out << "#     " << dump.id << " " << dump.style << " every " << dump.every
          << " path " << dump.path << '\n';
    }
  }
  out << "# end run configuration\n";
}

std::size_t Model::index(int ix, int iy, int iz) const {
  const int ny = grid_ny();
  const int nz = grid_nz();
  return (static_cast<std::size_t>(ix) * static_cast<std::size_t>(ny) +
          static_cast<std::size_t>(iy)) *
             static_cast<std::size_t>(nz) +
         static_cast<std::size_t>(iz);
}

double Model::voxel_dx() const {
  if (config_.geometry == GeometryKind::Slab) {
    return config_.slab.dx;
  }
  if (config_.geometry == GeometryKind::Sphere) {
    return config_.sphere.dx;
  }
  if (config_.geometry == GeometryKind::Tiff) {
    return config_.tiff.dx;
  }
  return 0.0;
}

int Model::grid_nx() const {
  if (config_.geometry == GeometryKind::Slab) {
    return config_.slab.nx;
  }
  if (config_.geometry == GeometryKind::Sphere) {
    return static_cast<int>(std::ceil(config_.sphere.diameter / config_.sphere.dx));
  }
  return config_.tiff.nx;
}

int Model::grid_ny() const {
  if (config_.geometry == GeometryKind::Slab) {
    return config_.slab.ny;
  }
  if (config_.geometry == GeometryKind::Tiff) {
    return config_.tiff.ny;
  }
  return grid_nx();
}

int Model::grid_nz() const {
  if (config_.geometry == GeometryKind::Slab) {
    return config_.slab.nz;
  }
  if (config_.geometry == GeometryKind::Tiff) {
    return config_.tiff.nz;
  }
  return grid_nx();
}

std::array<double, 3> Model::carryover_center() const {
  if (config_.geometry == GeometryKind::Sphere) {
    return {{0.0, 0.0, 0.0}};
  }
  const auto lo = real_domain_lo();
  const auto hi = real_domain_hi();
  return {{0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1]), 0.5 * (lo[2] + hi[2])}};
}

std::array<double, 3> Model::real_domain_lo() const {
  if (config_.geometry == GeometryKind::Slab) {
    return {{0.0, 0.0, 0.0}};
  }
  if (config_.geometry == GeometryKind::Tiff) {
    return config_.tiff.origin;
  }

  const double dx = voxel_dx();
  std::array<double, 3> lo{{0.0, 0.0, 0.0}};
  bool initialized = false;
  for (const auto &voxel : voxels_) {
    if (!voxel.active && voxel.remaining_mass <= 0.0) {
      continue;
    }
    const std::array<double, 3> corner{{voxel.x - 0.5 * dx, voxel.y - 0.5 * dx,
                                        voxel.z - 0.5 * dx}};
    if (!initialized) {
      lo = corner;
      initialized = true;
    } else {
      lo[0] = std::min(lo[0], corner[0]);
      lo[1] = std::min(lo[1], corner[1]);
      lo[2] = std::min(lo[2], corner[2]);
    }
  }
  return lo;
}

std::array<double, 3> Model::real_domain_hi() const {
  if (config_.geometry == GeometryKind::Slab) {
    const auto &g = config_.slab;
    return {{static_cast<double>(g.nx) * g.dx, static_cast<double>(g.ny) * g.dx,
             static_cast<double>(g.nz) * g.dx}};
  }
  if (config_.geometry == GeometryKind::Tiff) {
    const auto &g = config_.tiff;
    return {{g.origin[0] + static_cast<double>(g.nx) * g.dx,
             g.origin[1] + static_cast<double>(g.ny) * g.dx,
             g.origin[2] + static_cast<double>(g.nz) * g.dx}};
  }

  const double dx = voxel_dx();
  std::array<double, 3> hi{{0.0, 0.0, 0.0}};
  bool initialized = false;
  for (const auto &voxel : voxels_) {
    if (!voxel.active && voxel.remaining_mass <= 0.0) {
      continue;
    }
    const std::array<double, 3> corner{{voxel.x + 0.5 * dx, voxel.y + 0.5 * dx,
                                        voxel.z + 0.5 * dx}};
    if (!initialized) {
      hi = corner;
      initialized = true;
    } else {
      hi[0] = std::max(hi[0], corner[0]);
      hi[1] = std::max(hi[1], corner[1]);
      hi[2] = std::max(hi[2], corner[2]);
    }
  }
  return hi;
}

int Model::active_voxel_count() const {
  return static_cast<int>(std::count_if(voxels_.begin(), voxels_.end(),
                                       [](const Voxel &voxel) { return voxel.active; }));
}

int Model::deleted_voxel_count() const {
  return initial_active_voxels_ - active_voxel_count();
}

double Model::remaining_mass() const {
  double mass = 0.0;
  for (const auto &voxel : voxels_) {
    mass += voxel.remaining_mass;
  }
  return mass;
}

int Model::front_ix() const {
  int result = std::numeric_limits<int>::max();
  for (const auto &voxel : voxels_) {
    if (voxel.active) {
      result = std::min(result, voxel.ix);
    }
  }
  return result == std::numeric_limits<int>::max() ? -1 : result;
}

double Model::inferred_radius() const {
  if (config_.geometry != GeometryKind::Sphere) {
    return 0.0;
  }
  const double mass = remaining_mass();
  if (mass <= 0.0 || config_.material.density <= 0.0) {
    return 0.0;
  }
  return std::cbrt(3.0 * mass / (4.0 * std::acos(-1.0) * config_.material.density));
}

} // namespace iac
