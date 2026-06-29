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
  double molar_mass = 0.0;
  std::string formula;
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

struct TiffGeometry {
  std::string file;
  double dx = 0.0;
  std::string material;
  std::array<double, 3> origin{{0.0, 0.0, 0.0}};
  std::string axes = "xyz";
  std::string origin_mode = "center";
  int nx = 0;
  int ny = 0;
  int nz = 0;
};

enum class GeometryKind {
  None,
  Slab,
  Sphere,
  Tiff,
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

struct AblationCommand {
  std::string voxels;
  std::string source;
  std::string surface;
  std::string policy = "local";
  std::string face;
  bool delete_empty = true;
};

struct IsthmusSurfaceCommand {
  std::string name;
  std::string voxels;
  int buffer = 1;
  double resolution = 1.0;
  double iso_value = 0.5;
  bool weighting = true;
  bool map = true;
  bool crop_real = false;
};

struct SurfaceFluxCommand {
  std::string surface;
  std::string style = "source";
  std::string source;
  std::string select;
  std::string voxels;
  std::array<double, 3> direction{{0.0, 0.0, 1.0}};
  double min_cos = 0.0;
  double pressure = 0.0;
  double temperature = 0.0;
  double mole_fraction = 1.0;
  double molecular_mass = 0.0;
  double reaction_prob = 1.0;
  double solid_mass_per_hit = 0.0;
};

struct VoxelGhostCommand {
  std::string voxels;
  std::string axis;
  std::string side = "both";
  std::string boundary = "infinite";
  int layers = 1;
};

struct VoxelDump {
  std::string id;
  std::string voxels;
  std::string style;
  int every = 1;
  std::string path;
  std::string select = "active";
  std::string scalar = "mf";
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
  LimitTime,
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
  bool use_time_limit = false;
  double time_limit = 0.0;
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
  bool require_program = true;
  std::string units = "si";
  std::string voxel_name;
  GeometryKind geometry = GeometryKind::None;
  Material material;
  SlabGeometry slab;
  SphereGeometry sphere;
  TiffGeometry tiff;
  ConstantSource source;
  Timestep timestep;
  StatsConfig stats;
  std::vector<VoxelDump> dumps;
  std::vector<SurfaceDump> surface_dumps;
  RunConfig run;
  std::vector<VerificationCheck> checks;
  std::vector<ConvergenceCheck> convergence_checks;
  std::vector<VoxelGhostCommand> ghosts;
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
  double volume_fraction = 0.0;
  double front = 0.0;
  double radius = 0.0;
  double requested_mass_step = 0.0;
  double applied_mass_step = 0.0;
  double dropped_mass_step = 0.0;
};

} // namespace iac
