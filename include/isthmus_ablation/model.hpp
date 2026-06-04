#pragma once

#include "isthmus_ablation/types.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace iac {

class Model {
public:
  explicit Model(Config config);

  const Config &config() const { return config_; }
  const std::vector<HistoryRow> &history() const { return history_; }
  double timestep() const { return dt_; }
  int step_count() const { return current_step_; }

  void execute(std::ostream *stats = nullptr);
  void write_verification_csv(const std::string &path) const;
  void verify() const;

private:
  Config config_;
  std::vector<Voxel> voxels_;
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
  void derive_timestep();
  void validate_ablation(const AblationCommand &ablate, const std::string &context) const;
  void run_steps(const RunConfig &run, std::ostream *stats);
  void begin_step();
  void open_step();
  void advance_local_slab(const AblationCommand &ablate);
  void record_history(int step, double time);
  HistoryRow make_history_row(int step, double time) const;
  void write_scheduled_dumps(int step) const;
  void write_history_csv(const VoxelDump &dump) const;
  void write_vtu(const VoxelDump &dump, int step) const;
  void print_run_summary(std::ostream &out) const;
  void print_header(std::ostream &out) const;
  void print_row(std::ostream &out, const HistoryRow &row) const;

  std::size_t index(int ix, int iy, int iz) const;
  int active_voxel_count() const;
  int deleted_voxel_count() const;
  double remaining_mass() const;
  int front_ix() const;
};

} // namespace iac
