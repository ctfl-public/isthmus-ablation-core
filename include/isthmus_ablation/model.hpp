#pragma once

#include "isthmus_ablation/types.hpp"

#include <array>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace iac {

class Model {
public:
  struct SurfaceCellField {
    std::string name;
    std::vector<double> values;
  };

  struct PublicSurfaceTriangle {
    std::array<double, 3> a{{0.0, 0.0, 0.0}};
    std::array<double, 3> b{{0.0, 0.0, 0.0}};
    std::array<double, 3> c{{0.0, 0.0, 0.0}};
    std::array<double, 3> normal{{0.0, 0.0, 0.0}};
    double area = 0.0;
    double last_requested_mass = 0.0;
  };

  explicit Model(Config config);

  const Config &config() const { return config_; }
  const std::vector<HistoryRow> &history() const { return history_; }
  double timestep() const { return dt_; }
  int step_count() const { return current_step_; }
  std::vector<PublicSurfaceTriangle> surface_triangles(const std::string &name) const;

  void set_timestep(double dt);
  void set_timestep_from_source_courant(double courant, const std::string &source);
  void reset_run_state();
  void generate_surface(const IsthmusSurfaceCommand &surface);
  void apply_flux(const SurfaceFluxCommand &flux);
  void apply_triangle_fluxes(const std::string &surface, const std::vector<double> &mass_fluxes);
  void set_timestep_from_triangle_fluxes(const std::string &surface,
                                         const std::vector<double> &mass_fluxes,
                                         double courant);
  void ablate(const AblationCommand &ablate);
  void advance_steps(int steps);
  void advance_steps(int steps, std::ostream *stats);
  void execute(std::ostream *stats = nullptr);
  void write_history(const std::string &path) const;
  void write_voxels_vtu(const std::string &path, const std::string &select,
                        const std::string &scalar) const;
  void write_surface_vtp(const std::string &surface, const std::string &path) const;
  void write_surface_vtp(const std::string &surface, const std::string &path,
                         const std::vector<SurfaceCellField> &fields) const;
  void write_verification_csv(const std::string &path) const;
  void verify() const;
  double verification_error(const VerificationCheck &check) const;
  void set_diagnostic(const std::string &name, double value);
  double diagnostic(const std::string &name) const;
  double diagnostic_verification_error(const VerificationCheck &check) const;
  void verify_diagnostic(const VerificationCheck &check) const;
  bool has_diagnostic(const std::string &name) const;
  void print_run_summary_public(std::ostream &out) const;
  void print_stats_header(std::ostream &out) const;
  void print_latest_stats(std::ostream &out) const;

private:
  struct SurfaceTriangle {
    std::array<double, 3> a{{0.0, 0.0, 0.0}};
    std::array<double, 3> b{{0.0, 0.0, 0.0}};
    std::array<double, 3> c{{0.0, 0.0, 0.0}};
    std::array<double, 3> normal{{0.0, 0.0, 0.0}};
    double area = 0.0;
    std::vector<std::size_t> voxel_ids;
    std::vector<double> fractions;
    double requested_mass = 0.0;
    double last_requested_mass = 0.0;
  };

  struct SurfaceState {
    std::string name;
    std::vector<SurfaceTriangle> triangles;
  };

  struct SurfaceVoxelRecord {
    const Voxel *voxel = nullptr;
    int ix = 0;
    int iy = 0;
    int iz = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool real = true;
  };

  Config config_;
  std::vector<Voxel> voxels_;
  std::unordered_map<std::string, SurfaceState> surfaces_;
  std::unordered_map<std::string, double> diagnostics_;
  std::vector<HistoryRow> history_;
  double voxel_mass_ = 0.0;
  double initial_mass_ = 0.0;
  int initial_active_voxels_ = 0;
  double dt_ = 0.0;
  int current_step_ = 0;
  double current_time_ = 0.0;
  double applied_mass_total_ = 0.0;
  double requested_mass_step_ = 0.0;
  double applied_mass_step_ = 0.0;
  double dropped_mass_step_ = 0.0;
  bool step_open_ = false;

  void validate_and_initialize();
  void initialize_slab();
  void initialize_sphere();
  void initialize_tiff();
  void derive_timestep();
  void validate_ablation(const AblationCommand &ablate, const std::string &context) const;
  void validate_isthmus_surface(const IsthmusSurfaceCommand &surface) const;
  void validate_surface_flux(const SurfaceFluxCommand &flux) const;
  void validate_ghosts() const;
  void run_steps(const RunConfig &run, std::ostream *stats);
  void begin_step();
  void open_step();
  void advance_local_slab(const AblationCommand &ablate);
  void advance_surface_ablation(const AblationCommand &ablate);
  void apply_surface_local_increments(const std::vector<double> &mass_increments,
                                      const AblationCommand &ablate);
  void apply_surface_normal_carryover(const std::vector<double> &mass_increments,
                                      const AblationCommand &ablate);
  void generate_isthmus_surface(const IsthmusSurfaceCommand &surface);
  std::vector<SurfaceVoxelRecord> surface_voxel_records() const;
  void apply_surface_flux(const SurfaceFluxCommand &flux);
  void record_history(int step, double time);
  HistoryRow make_history_row(int step, double time) const;
  void write_scheduled_dumps(int step) const;
  void write_history_csv(const VoxelDump &dump) const;
  void write_vtu(const VoxelDump &dump, int step) const;
  void write_vtp(const SurfaceDump &dump, int step,
                 const std::vector<SurfaceCellField> &fields = {}) const;
  void print_run_summary(std::ostream &out) const;
  void print_header(std::ostream &out) const;
  void print_row(std::ostream &out, const HistoryRow &row) const;

  std::size_t index(int ix, int iy, int iz) const;
  double voxel_dx() const;
  int grid_nx() const;
  int grid_ny() const;
  int grid_nz() const;
  std::array<double, 3> carryover_center() const;
  std::array<double, 3> real_domain_lo() const;
  std::array<double, 3> real_domain_hi() const;
  int active_voxel_count() const;
  int deleted_voxel_count() const;
  double remaining_mass() const;
  int front_ix() const;
  double inferred_radius() const;
};

} // namespace iac
