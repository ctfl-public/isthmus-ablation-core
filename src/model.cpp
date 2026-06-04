#include "isthmus_ablation/model.hpp"

#include "isthmus_ablation/expression.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace iac {
namespace {

constexpr double kEpsilon = 1.0e-12;

std::string format_error(const std::string &quantity, double error, double tolerance) {
  std::ostringstream out;
  out << "verification failed for '" << quantity << "': error " << std::setprecision(17)
      << error << " exceeds tolerance " << tolerance;
  return out.str();
}

const std::vector<std::string> &default_stats_columns() {
  static const std::vector<std::string> columns{
      "step", "time", "active-voxels", "deleted-voxels", "remaining-mass",
      "mass-fraction", "front"};
  return columns;
}

double history_value(const HistoryRow &row, const std::string &column) {
  if (column == "step") {
    return static_cast<double>(row.step);
  }
  if (column == "time") {
    return row.time;
  }
  if (column == "active-voxels") {
    return static_cast<double>(row.active_voxels);
  }
  if (column == "deleted-voxels") {
    return static_cast<double>(row.deleted_voxels);
  }
  if (column == "remaining-mass" || column == "mass") {
    return row.remaining_mass;
  }
  if (column == "mass-fraction") {
    return row.mass_fraction;
  }
  if (column == "front") {
    return row.front;
  }
  if (column == "requested-mass-step") {
    return row.requested_mass_step;
  }
  if (column == "applied-mass-step") {
    return row.applied_mass_step;
  }
  if (column == "dropped-mass-step") {
    return row.dropped_mass_step;
  }
  throw RuntimeError("unknown stats_style column '" + column + "'");
}

int stats_column_width(const std::string &column) {
  return std::max(12, static_cast<int>(column.size())) + 1;
}

std::string normalize_quantity(std::string quantity) {
  if (quantity == "mass") {
    return "remaining-mass";
  }
  return quantity;
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

} // namespace

Model::Model(Config config) : config_(std::move(config)) {
  validate_and_initialize();
}

void Model::validate_and_initialize() {
  if (config_.units != "si") {
    throw RuntimeError("only 'units si' is currently supported");
  }
  if (config_.voxel_name.empty()) {
    throw RuntimeError("missing voxel model; expected 'voxel create <name> slab ...'");
  }
  if (config_.material.name.empty() || config_.material.density <= 0.0) {
    throw RuntimeError("voxel material requires a positive density");
  }
  if (config_.slab.material != config_.material.name) {
    throw RuntimeError("voxel create references unknown material '" + config_.slab.material + "'");
  }
  if (config_.source.name.empty() || config_.source.value < 0.0) {
    throw RuntimeError("source constant requires a nonnegative value");
  }
  if (!config_.fix.name.empty()) {
    validate_ablation(
        AblationCommand{config_.fix.voxels, config_.fix.source, config_.fix.policy,
                        config_.fix.delete_empty},
        "fix '" + config_.fix.name + "'");
    if (config_.fix.every <= 0) {
      throw RuntimeError("fix every value must be positive");
    }
  }
  bool has_run = false;
  bool has_ablation = !config_.fix.name.empty();
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
    }
  }
  if (!has_run) {
    throw RuntimeError("input must contain at least one run command");
  }
  if (!has_ablation) {
    throw RuntimeError("input must contain a voxel ablate command or fix voxel/ablate");
  }

  voxel_mass_ = config_.material.density * std::pow(config_.slab.dx, 3);
  initialize_slab();
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
        voxels_.push_back(Voxel{id++, ix, iy, iz, voxel_mass_, true, false});
      }
    }
  }

  initial_mass_ = voxel_mass_ * static_cast<double>(voxels_.size());
  initial_active_voxels_ = static_cast<int>(voxels_.size());
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
      config_.timestep.courant * config_.material.density * config_.slab.dx /
      config_.source.value;
  dt_ = safe_dt;
}

void Model::validate_ablation(const AblationCommand &ablate, const std::string &context) const {
  if (ablate.voxels != config_.voxel_name) {
    throw RuntimeError(context + " references unknown voxel model '" + ablate.voxels + "'");
  }
  if (ablate.source != config_.source.name) {
    throw RuntimeError(context + " references unknown source '" + ablate.source + "'");
  }
  if (ablate.policy != "local") {
    throw RuntimeError("only local ablation policy is currently implemented");
  }
}

void Model::execute(std::ostream *stats) {
  history_.clear();
  current_step_ = 0;
  current_time_ = 0.0;
  step_open_ = false;
  applied_mass_total_ = 0.0;
  record_history(0, 0.0);
  write_scheduled_dumps(0);
  if (stats != nullptr) {
    print_run_summary(*stats);
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
      } else {
        const auto it = labels.find(command.target);
        if (it == labels.end()) {
          throw RuntimeError("jump references unknown label '" + command.target + "'");
        }
        pc = it->second;
      }
      break;
    case CommandKind::Run:
      run_steps(command.run, stats);
      ++pc;
      break;
    case CommandKind::VoxelAblate:
      open_step();
      advance_local_slab(command.ablate);
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
  for (int i = 0; i < steps; ++i) {
    open_step();
    const int next_step = current_step_ + 1;
    if (!config_.fix.name.empty() && next_step % config_.fix.every == 0) {
      advance_local_slab(AblationCommand{config_.fix.voxels, config_.fix.source,
                                         config_.fix.policy, config_.fix.delete_empty});
    }
    current_step_ = next_step;
    current_time_ = static_cast<double>(current_step_) * dt_;
    record_history(current_step_, current_time_);
    write_scheduled_dumps(current_step_);
    step_open_ = false;
    if (stats != nullptr && current_step_ % config_.stats.every == 0) {
      print_row(*stats, history_.back());
    }
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
  for (int iy = 0; iy < g.ny; ++iy) {
    for (int iz = 0; iz < g.nz; ++iz) {
      int column_front = -1;
      for (int ix = 0; ix < g.nx; ++ix) {
        if (voxels_[index(ix, iy, iz)].active) {
          column_front = ix;
          break;
        }
      }
      if (column_front < 0) {
        continue;
      }

      double requested = config_.source.value * face_area * dt_;
      requested_mass_step_ += requested;
      auto &voxel = voxels_[index(column_front, iy, iz)];
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
}

void Model::record_history(int step, double time) {
  history_.push_back(make_history_row(step, time));
}

HistoryRow Model::make_history_row(int step, double time) const {
  const auto &g = config_.slab;
  const double remaining = remaining_mass();
  const int front = front_ix();
  const double discrete_front = front < 0 ? static_cast<double>(g.nx) * g.dx
                                          : static_cast<double>(front) * g.dx;

  HistoryRow row;
  row.step = step;
  row.time = time;
  row.active_voxels = active_voxel_count();
  row.deleted_voxels = deleted_voxel_count();
  row.remaining_mass = remaining;
  row.mass_fraction = initial_mass_ > 0.0 ? remaining / initial_mass_ : 0.0;
  row.front = discrete_front;
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
}

void Model::write_history_csv(const VoxelDump &dump) const {
  ensure_parent_directory(dump.path);
  std::ofstream out(dump.path);
  if (!out) {
    throw RuntimeError("could not write history file '" + dump.path + "'");
  }
  out << "step,time,active-voxels,deleted-voxels,remaining-mass,"
      << "mass-fraction,front,requested-mass-step,applied-mass-step,dropped-mass-step\n";
  out << std::setprecision(17);
  for (const auto &row : history_) {
    out << row.step << ',' << row.time << ',' << row.active_voxels << ','
        << row.deleted_voxels << ',' << row.remaining_mass << ',' << row.mass_fraction
        << ',' << row.front << ',' << row.requested_mass_step << ','
        << row.applied_mass_step << ',' << row.dropped_mass_step << '\n';
  }
}

void Model::write_vtu(const VoxelDump &dump, int step) const {
  const auto path = dump_path_for_step(dump.path, step);
  ensure_parent_directory(path);
  std::ofstream out(path);
  if (!out) {
    throw RuntimeError("could not write VTU file '" + path + "'");
  }

  std::vector<const Voxel *> selected;
  selected.reserve(voxels_.size());
  for (const auto &voxel : voxels_) {
    if (dump.select == "active" && !voxel.active) {
      continue;
    }
    selected.push_back(&voxel);
  }

  const auto &g = config_.slab;
  out << std::setprecision(17);
  out << "<?xml version=\"1.0\"?>\n";
  out << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  out << "  <UnstructuredGrid>\n";
  out << "    <Piece NumberOfPoints=\"" << selected.size() * 8 << "\" NumberOfCells=\""
      << selected.size() << "\">\n";

  out << "      <Points>\n";
  out << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    const double x0 = static_cast<double>(voxel->ix) * g.dx;
    const double y0 = static_cast<double>(voxel->iy) * g.dx;
    const double z0 = static_cast<double>(voxel->iz) * g.dx;
    const double x1 = x0 + g.dx;
    const double y1 = y0 + g.dx;
    const double z1 = z0 + g.dx;
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
  out << "        <DataArray type=\"Float64\" Name=\"mass-fraction\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << (voxel_mass_ > 0.0 ? voxel->remaining_mass / voxel_mass_ : 0.0)
        << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Float64\" Name=\"remaining-mass\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << voxel->remaining_mass << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int64\" Name=\"id\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << voxel->id << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"ix\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << voxel->ix << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"iy\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << voxel->iy << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"iz\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << voxel->iz << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"active\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << (voxel->active ? 1 : 0) << '\n';
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int32\" Name=\"fixed\" format=\"ascii\">\n";
  for (const auto *voxel : selected) {
    out << "          " << (voxel->fixed ? 1 : 0) << '\n';
  }
  out << "        </DataArray>\n";
  out << "      </CellData>\n";
  out << "    </Piece>\n";
  out << "  </UnstructuredGrid>\n";
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
  out << "quantity,expression,step,time,actual,exact,error,tolerance,norm,pass\n";
  out << std::setprecision(17);
  for (const auto &check : config_.checks) {
    const std::string quantity = normalize_quantity(check.quantity);
    for (const auto &row : history_) {
      const auto &g = config_.slab;
      const double length = static_cast<double>(g.nx) * g.dx;
      const double area = static_cast<double>(g.ny) * static_cast<double>(g.nz) * g.dx * g.dx;
      std::unordered_map<std::string, double> variables{
          {"step", static_cast<double>(row.step)},
          {"time", row.time},
          {"t", row.time},
          {"dt", dt_},
          {"rho", config_.material.density},
          {"density", config_.material.density},
          {"dx", g.dx},
          {"nx", static_cast<double>(g.nx)},
          {"ny", static_cast<double>(g.ny)},
          {"nz", static_cast<double>(g.nz)},
          {"length", length},
          {"area", area},
          {"initial-mass", initial_mass_},
          {"initial_mass", initial_mass_},
          {"voxel-mass", voxel_mass_},
          {"voxel_mass", voxel_mass_},
          {"active-voxels", static_cast<double>(row.active_voxels)},
          {"active_voxels", static_cast<double>(row.active_voxels)},
          {"deleted-voxels", static_cast<double>(row.deleted_voxels)},
          {"deleted_voxels", static_cast<double>(row.deleted_voxels)},
          {"remaining-mass", row.remaining_mass},
          {"remaining_mass", row.remaining_mass},
          {"mass", row.remaining_mass},
          {"mass-fraction", row.mass_fraction},
          {"mass_fraction", row.mass_fraction},
          {"front", row.front},
          {"requested-mass-step", row.requested_mass_step},
          {"requested_mass_step", row.requested_mass_step},
          {"applied-mass-step", row.applied_mass_step},
          {"applied_mass_step", row.applied_mass_step},
          {"dropped-mass-step", row.dropped_mass_step},
          {"dropped_mass_step", row.dropped_mass_step},
          {config_.source.name, config_.source.value},
          {"source:" + config_.source.name, config_.source.value},
      };
      const double actual = history_value(row, quantity);
      const double exact = evaluate_expression(check.expression, variables);
      const double error = std::abs(actual - exact);
      out << csv_escape(check.quantity) << ',' << csv_escape(check.expression) << ','
          << row.step << ',' << row.time << ',' << actual << ',' << exact << ','
          << error << ',' << check.tolerance << ',' << csv_escape(check.norm) << ','
          << (error <= check.tolerance ? "yes" : "no") << '\n';
    }
  }
}

void Model::verify() const {
  if (history_.empty()) {
    throw RuntimeError("cannot verify before run");
  }
  for (const auto &check : config_.checks) {
    const std::string quantity = normalize_quantity(check.quantity);
    double accumulated = 0.0;
    double max_error = 0.0;
    int count = 0;
    const auto evaluate_row = [&](const HistoryRow &row) {
      const auto &g = config_.slab;
      const double length = static_cast<double>(g.nx) * g.dx;
      const double area = static_cast<double>(g.ny) * static_cast<double>(g.nz) * g.dx * g.dx;
      std::unordered_map<std::string, double> variables{
          {"step", static_cast<double>(row.step)},
          {"time", row.time},
          {"t", row.time},
          {"dt", dt_},
          {"rho", config_.material.density},
          {"density", config_.material.density},
          {"dx", g.dx},
          {"nx", static_cast<double>(g.nx)},
          {"ny", static_cast<double>(g.ny)},
          {"nz", static_cast<double>(g.nz)},
          {"length", length},
          {"area", area},
          {"initial-mass", initial_mass_},
          {"initial_mass", initial_mass_},
          {"voxel-mass", voxel_mass_},
          {"voxel_mass", voxel_mass_},
          {"active-voxels", static_cast<double>(row.active_voxels)},
          {"active_voxels", static_cast<double>(row.active_voxels)},
          {"deleted-voxels", static_cast<double>(row.deleted_voxels)},
          {"deleted_voxels", static_cast<double>(row.deleted_voxels)},
          {"remaining-mass", row.remaining_mass},
          {"remaining_mass", row.remaining_mass},
          {"mass", row.remaining_mass},
          {"mass-fraction", row.mass_fraction},
          {"mass_fraction", row.mass_fraction},
          {"front", row.front},
          {"requested-mass-step", row.requested_mass_step},
          {"requested_mass_step", row.requested_mass_step},
          {"applied-mass-step", row.applied_mass_step},
          {"applied_mass_step", row.applied_mass_step},
          {"dropped-mass-step", row.dropped_mass_step},
          {"dropped_mass_step", row.dropped_mass_step},
          {config_.source.name, config_.source.value},
          {"source:" + config_.source.name, config_.source.value},
      };
      const double actual = history_value(row, quantity);
      const double exact = evaluate_expression(check.expression, variables);
      return std::abs(actual - exact);
    };

    if (check.norm == "final") {
      max_error = evaluate_row(history_.back());
      count = 1;
    } else if (check.norm == "max" || check.norm == "rms") {
      for (const auto &row : history_) {
        const double error = evaluate_row(row);
        max_error = std::max(max_error, error);
        accumulated += error * error;
        ++count;
      }
    } else {
      throw RuntimeError("unknown verify norm '" + check.norm + "'");
    }

    double error = max_error;
    if (check.norm == "rms") {
      error = count > 0 ? std::sqrt(accumulated / static_cast<double>(count)) : 0.0;
    }
    if (!(error <= check.tolerance)) {
      throw RuntimeError(format_error(check.quantity, error, check.tolerance));
    }
  }
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
    if (columns[i] == "step" || columns[i] == "active-voxels" ||
        columns[i] == "deleted-voxels") {
      out << std::setw(width) << static_cast<long long>(value);
    } else {
      out << std::setw(width) << std::defaultfloat << value;
    }
  }
  out << '\n';
}

void Model::print_run_summary(std::ostream &out) const {
  const auto &g = config_.slab;
  out << "# Standalone voxel ablation\n";
  out << "# run configuration\n";
  out << "#   voxel model = " << config_.voxel_name << '\n';
  out << "#   geometry = slab\n";
  out << "#   grid = " << g.nx << " x " << g.ny << " x " << g.nz << '\n';
  out << "#   dx = " << std::setprecision(8) << g.dx << " m\n";
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
  if (config_.fix.name.empty()) {
    out << "#   fix = off\n";
  } else {
    out << "#   fix = " << config_.fix.name << " voxel/ablate every " << config_.fix.every
        << " policy " << config_.fix.policy << " delete "
        << (config_.fix.delete_empty ? "yes" : "no") << '\n';
  }
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
  out << "# end run configuration\n";
}

std::size_t Model::index(int ix, int iy, int iz) const {
  const auto &g = config_.slab;
  return (static_cast<std::size_t>(ix) * static_cast<std::size_t>(g.ny) +
          static_cast<std::size_t>(iy)) *
             static_cast<std::size_t>(g.nz) +
         static_cast<std::size_t>(iz);
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

} // namespace iac
