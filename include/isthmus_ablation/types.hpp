#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace iac {

struct InputError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct RuntimeError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Material {
  std::string name;
  double density = 0.0;
};

struct SlabGeometry {
  int nx = 0;
  int ny = 0;
  int nz = 0;
  double dx = 0.0;
  std::string material;
};

struct SphereGeometry {
  double diameter = 0.0;
  double dx = 0.0;
  int resolution = 0;
  std::string material;
};

enum class GeometryKind {
  None,
  Slab,
  Sphere,
};

struct ConstantSource {
  std::string name;
  double value = 0.0;
  std::string units;
};

enum class TimestepKind {
  Explicit,
  MassCourant,
};

struct Timestep {
  TimestepKind kind = TimestepKind::Explicit;
  double value = 0.0;
  double courant = 0.0;
  std::string source;
};

struct AblationFix {
  std::string name;
  int every = 1;
  std::string voxels;
  std::string source;
  std::string policy = "local";
  bool delete_empty = true;
};

struct AblationCommand {
  std::string voxels;
  std::string source;
  std::string surface;
  std::string policy = "local";
  bool delete_empty = true;
};

struct IsthmusSurfaceCommand {
  std::string name;
  std::string voxels;
  int buffer = 1;
  bool weighting = false;
  bool map = true;
};

struct SurfaceFluxCommand {
  std::string surface;
  std::string source;
  std::string select = "all";
  std::string voxels;
  std::array<double, 3> direction{{0.0, 0.0, 1.0}};
  double min_cos = 0.0;
};

struct VoxelDump {
  std::string id;
  std::string voxels;
  std::string style;
  int every = 1;
  std::string path;
  std::string select = "active";
  std::string scalar = "mass-fraction";
};

struct SurfaceDump {
  std::string id;
  std::string surface;
  std::string style;
  int every = 1;
  std::string path;
};

struct StatsConfig {
  int every = 1;
  std::vector<std::string> columns;
};

struct RunConfig {
  bool use_duration = false;
  double duration = 0.0;
  int steps = 0;
};

enum class CommandKind {
  Label,
  VariableLoop,
  Next,
  Jump,
  Run,
  VoxelAblate,
  IsthmusSurface,
  SurfaceFlux,
};

struct ScriptCommand {
  CommandKind kind = CommandKind::Run;
  std::string name;
  std::string target;
  int count = 0;
  RunConfig run;
  AblationCommand ablate;
  IsthmusSurfaceCommand isthmus_surface;
  SurfaceFluxCommand surface_flux;
};

struct VerificationCheck {
  std::string quantity;
  std::string expression;
  double tolerance = 0.0;
  std::string tolerance_mode = "absolute";
  std::string norm = "final";
};

struct ConvergenceVariable {
  std::string name;
  std::vector<std::string> values;
};

struct ConvergenceCheck {
  VerificationCheck check;
  std::vector<ConvergenceVariable> variables;
  double min_order = 0.0;
  double max_order = 10.0;
  bool require_monotonic = true;
};

struct Config {
  std::string units = "si";
  std::string voxel_name;
  GeometryKind geometry = GeometryKind::None;
  Material material;
  SlabGeometry slab;
  SphereGeometry sphere;
  ConstantSource source;
  Timestep timestep;
  AblationFix fix;
  StatsConfig stats;
  std::vector<VoxelDump> dumps;
  std::vector<SurfaceDump> surface_dumps;
  RunConfig run;
  std::vector<VerificationCheck> checks;
  std::vector<ConvergenceCheck> convergence_checks;
  std::vector<ScriptCommand> program;
};

struct Voxel {
  std::size_t id = 0;
  int ix = 0;
  int iy = 0;
  int iz = 0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  double remaining_mass = 0.0;
  bool active = true;
  bool fixed = false;
};

struct HistoryRow {
  int step = 0;
  double time = 0.0;
  int active_voxels = 0;
  int deleted_voxels = 0;
  double remaining_mass = 0.0;
  double mass_fraction = 0.0;
  double front = 0.0;
  double radius = 0.0;
  double requested_mass_step = 0.0;
  double applied_mass_step = 0.0;
  double dropped_mass_step = 0.0;
};

} // namespace iac
