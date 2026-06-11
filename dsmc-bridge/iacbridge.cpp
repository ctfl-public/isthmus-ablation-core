#include "iacbridge.h"

#include "comm.h"
#include "domain.h"
#include "error.h"
#include "grid.h"
#include "memory.h"
#include "mixture.h"
#include "output.h"
#include "particle.h"
#include "sparta.h"
#include "stats.h"
#include "surf.h"
#include "surf_collide.h"
#include "surf_react.h"
#include "update.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace SPARTA_NS {
namespace IACBridge {
namespace {

enum { PART_NONE, PART_CHECK, PART_KEEP };
enum { CELL_UNKNOWN, CELL_OUTSIDE, CELL_INSIDE, CELL_OVERLAP };
enum { OUTPUT_NONE, OUTPUT_IAC, OUTPUT_SPA };

std::unique_ptr<iac::Model> active_model;
iac::Config active_config;
std::unordered_map<std::string, std::vector<iac::Model::PublicSurfaceTriangle>> surface_cache;
bigint last_coupling_step = 0;
bool stats_header_printed = false;
int last_compact_output = OUTPUT_NONE;

struct GridVtuDumpInfo {
  std::string fix_id;
  std::string path;
  std::string index_mode;
  std::vector<std::string> fields;
};

std::vector<GridVtuDumpInfo> grid_vtu_dumps;

void initialize_defaults() {
  active_config.require_program = false;
  active_config.units = "si";
  active_config.source.name = "dsmc";
  active_config.source.value = 0.0;
  active_config.timestep.kind = iac::TimestepKind::Explicit;
  active_config.timestep.value = 1.0;
}

void require_grid(SPARTA *sparta) {
  if (!sparta->grid->exist) {
    sparta->error->all(FLERR, "Cannot install isthmus-ablation surface before grid is defined");
  }
  if (sparta->surf->implicit) {
    sparta->error->all(FLERR, "Cannot install isthmus-ablation surface with implicit surfs");
  }
}

void broadcast_surface_cache(SPARTA *sparta, const std::string &surface_id) {
  Comm *comm = sparta->comm;
  int count = 0;
  if (comm->me == 0) {
    const auto found = surface_cache.find(surface_id);
    if (found == surface_cache.end()) {
      throw std::runtime_error("IAC surface cache is missing generated surface '" + surface_id + "'");
    }
    count = static_cast<int>(found->second.size());
  }
  MPI_Bcast(&count, 1, MPI_INT, 0, sparta->world);
  if (count <= 0) {
    throw std::runtime_error("IAC generated surface is empty");
  }

  std::vector<double> flat(static_cast<std::size_t>(count) * 14, 0.0);
  if (comm->me == 0) {
    const auto &triangles = surface_cache.at(surface_id);
    for (int i = 0; i < count; ++i) {
      const auto &tri = triangles[static_cast<std::size_t>(i)];
      double *row = flat.data() + static_cast<std::size_t>(i) * 14;
      std::copy(tri.a.begin(), tri.a.end(), row);
      std::copy(tri.b.begin(), tri.b.end(), row + 3);
      std::copy(tri.c.begin(), tri.c.end(), row + 6);
      std::copy(tri.normal.begin(), tri.normal.end(), row + 9);
      row[12] = tri.area;
      row[13] = tri.last_requested_mass;
    }
  }
  MPI_Bcast(flat.data(), static_cast<int>(flat.size()), MPI_DOUBLE, 0, sparta->world);

  if (comm->me != 0) {
    auto &triangles = surface_cache[surface_id];
    triangles.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      auto &tri = triangles[static_cast<std::size_t>(i)];
      const double *row = flat.data() + static_cast<std::size_t>(i) * 14;
      std::copy(row, row + 3, tri.a.begin());
      std::copy(row + 3, row + 6, tri.b.begin());
      std::copy(row + 6, row + 9, tri.c.begin());
      std::copy(row + 9, row + 12, tri.normal.begin());
      tri.area = row[12];
      tri.last_requested_mass = row[13];
    }
  }
}

} // namespace

iac::Config &config() {
  static bool initialized = false;
  if (!initialized) {
    initialize_defaults();
    initialized = true;
  }
  return active_config;
}

bool owns_model(SPARTA *sparta) {
  return sparta->comm->me == 0;
}

bool has_model() {
  return static_cast<bool>(active_model);
}

iac::Model &model(SPARTA *sparta) {
  if (!owns_model(sparta)) {
    throw std::runtime_error("IAC model state is owned by MPI rank 0");
  }
  if (!active_model) {
    auto &cfg = config();
    if (sparta->update->dt > 0.0) {
      cfg.timestep.value = sparta->update->dt;
    }
    active_model.reset(new iac::Model(cfg));
    active_model->reset_run_state();
    last_coupling_step = sparta->update->ntimestep;
  }
  return *active_model;
}

void reset_model() {
  active_model.reset();
  surface_cache.clear();
  grid_vtu_dumps.clear();
  reset_stats_output();
}

void set_coupling_interval_from_dsmc(SPARTA *sparta) {
  if (!owns_model(sparta)) {
    last_coupling_step = sparta->update->ntimestep;
    return;
  }
  auto &m = model(sparta);
  const bigint current = sparta->update->ntimestep;
  bigint elapsed_steps = current - last_coupling_step;
  if (elapsed_steps <= 0) {
    elapsed_steps = 1;
  }
  m.set_timestep(static_cast<double>(elapsed_steps) * sparta->update->dt);
  last_coupling_step = current;
}

void set_last_coupling_step(SPARTA *sparta) {
  if (owns_model(sparta)) {
    model(sparta);
  }
  last_coupling_step = sparta->update->ntimestep;
}

void reset_stats_output() {
  stats_header_printed = false;
  last_compact_output = OUTPUT_NONE;
}

std::string boundary_name(int flag) {
  switch (flag) {
  case 0:
    return "periodic";
  case 1:
    return "outflow";
  case 2:
    return "reflect";
  case 3:
    return "surface";
  case 4:
    return "axisymmetric";
  default:
    return "unknown";
  }
}

std::string join_strings(const std::vector<std::string> &values, const std::string &separator) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << separator;
    }
    out << values[i];
  }
  return out.str();
}

bool stream_is_terminal(FILE *stream) {
  if (!stream) {
    return false;
  }
#if defined(_WIN32)
  return _isatty(_fileno(stream)) != 0;
#else
  return isatty(fileno(stream)) != 0;
#endif
}

bool terminal_color_enabled(FILE *stream) {
  const char *mode = std::getenv("IAC_COLOR");
  const std::string value = mode ? mode : "auto";
  if (value == "off" || value == "0" || value == "false") {
    return false;
  }
  if (value == "on" || value == "1" || value == "true") {
    return true;
  }
  return stream_is_terminal(stream);
}

std::string colorize_console_text(const std::string &text) {
  constexpr const char *kReset = "\033[0m";
  constexpr const char *kIac = "\033[32m";
  constexpr const char *kSpa = "\033[34m";

  std::ostringstream out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("[IAC]", 0) == 0) {
      out << kIac << line << kReset;
    } else if (line.rfind("[SPA]", 0) == 0) {
      out << kSpa << line << kReset;
    } else {
      out << line;
    }
    out << '\n';
  }
  return out.str();
}

void write_to_dsmc_outputs(SPARTA *sparta, const std::string &text) {
  if (sparta->comm->me != 0) {
    return;
  }
  FILE *console = sparta->screen ? sparta->screen : stdout;
  const std::string console_text = terminal_color_enabled(console) ? colorize_console_text(text) : text;
  if (sparta->screen) {
    std::fprintf(sparta->screen, "%s", console_text.c_str());
  } else {
    std::fprintf(stdout, "%s", console_text.c_str());
    std::fflush(stdout);
  }
  if (sparta->logfile) {
    std::fprintf(sparta->logfile, "%s", text.c_str());
  }
}

void record_grid_vtu_dump(const std::string &fix_id, const std::string &path,
                          const std::string &index_mode,
                          const std::vector<std::string> &fields) {
  for (auto &dump : grid_vtu_dumps) {
    if (dump.fix_id == fix_id && dump.path == path && dump.index_mode == index_mode) {
      dump.fields = fields;
      return;
    }
  }
  grid_vtu_dumps.push_back(GridVtuDumpInfo{fix_id, path, index_mode, fields});
}

std::string prefixed_lines(const std::string &text, const std::string &prefix,
                           bool strip_comment_marker) {
  std::ostringstream out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    if (strip_comment_marker && line[0] == '#') {
      line.erase(0, 1);
      if (!line.empty() && line[0] == ' ') {
        line.erase(0, 1);
      }
    }
    out << prefix << line << '\n';
  }
  return out.str();
}

std::vector<std::string> tokens_from_line(std::string line, bool strip_comment_marker) {
  if (strip_comment_marker && !line.empty() && line[0] == '#') {
    line.erase(0, 1);
  }
  std::istringstream in(line);
  std::vector<std::string> tokens;
  std::string token;
  while (in >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string prefixed_stats_table(const std::string &text, const std::string &prefix,
                                 bool strip_comment_marker,
                                 std::size_t first_column_min_width = 12,
                                 bool include_header = true) {
  std::vector<std::vector<std::string>> rows;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    auto tokens = tokens_from_line(line, strip_comment_marker);
    if (!tokens.empty()) {
      rows.push_back(std::move(tokens));
    }
  }
  if (rows.empty()) {
    return "";
  }
  if (!include_header && rows.size() > 1) {
    rows.erase(rows.begin());
  }

  std::size_t columns = 0;
  for (const auto &row : rows) {
    columns = std::max(columns, row.size());
  }
  std::vector<std::size_t> widths(columns, 0);
  for (const auto &row : rows) {
    for (std::size_t i = 0; i < row.size(); ++i) {
      widths[i] = std::max(widths[i], row[i].size());
    }
  }
  for (auto &width : widths) {
    width = std::max<std::size_t>(width + 1, 12);
  }
  if (!widths.empty()) {
    widths[0] = std::max<std::size_t>(widths[0] - 7, first_column_min_width);
  }

  std::ostringstream out;
  for (const auto &row : rows) {
    out << prefix;
    for (std::size_t i = 0; i < row.size(); ++i) {
      out << std::setw(static_cast<int>(widths[i])) << row[i];
    }
    out << '\n';
  }
  return out.str();
}

std::string read_stream(FILE *stream) {
  std::string text;
  std::rewind(stream);
  char buffer[4096];
  while (std::fgets(buffer, sizeof(buffer), stream)) {
    text += buffer;
  }
  return text;
}

void print_dsmc_summary(SPARTA *sparta, std::ostream &out) {
  out << "# DSMC/SPARTA configuration\n";
  if (sparta->domain->box_exist) {
    out << "#   box x = [" << std::setprecision(8) << sparta->domain->boxlo[0] << ", "
        << sparta->domain->boxhi[0] << "] m, length " << sparta->domain->xprd << " m\n";
    out << "#   box y = [" << sparta->domain->boxlo[1] << ", " << sparta->domain->boxhi[1]
        << "] m, length " << sparta->domain->yprd << " m\n";
    out << "#   box z = [" << sparta->domain->boxlo[2] << ", " << sparta->domain->boxhi[2]
        << "] m, length " << sparta->domain->zprd << " m\n";
  } else {
    out << "#   box = not created\n";
  }
  out << "#   boundary x = " << boundary_name(sparta->domain->bflag[0]) << " / "
      << boundary_name(sparta->domain->bflag[1]) << '\n';
  out << "#   boundary y = " << boundary_name(sparta->domain->bflag[2]) << " / "
      << boundary_name(sparta->domain->bflag[3]) << '\n';
  out << "#   boundary z = " << boundary_name(sparta->domain->bflag[4]) << " / "
      << boundary_name(sparta->domain->bflag[5]) << '\n';
  out << "#   DSMC timestep = " << std::setprecision(8) << sparta->update->dt << " s\n";
  out << "#   particle weight fnum = " << sparta->update->fnum << '\n';
  out << "#   reference number density nrho = " << sparta->update->nrho << " 1/m^3\n";
  if (sparta->grid->exist) {
    out << "#   grid cells = " << static_cast<long long>(sparta->grid->ncell);
    if (sparta->grid->uniform) {
      out << " (" << sparta->grid->unx << " x " << sparta->grid->uny << " x "
          << sparta->grid->unz << ")";
    }
    out << '\n';
  }
  out << "#   installed surface elements = " << static_cast<long long>(sparta->surf->nsurf)
      << '\n';
  out << "#   current local particles = " << sparta->particle->nlocal << '\n';

  out << "#   species = ";
  if (sparta->particle->nspecies <= 0) {
    out << "none\n";
  } else {
    for (int i = 0; i < sparta->particle->nspecies; ++i) {
      if (i != 0) {
        out << ", ";
      }
      out << sparta->particle->species[i].id;
    }
    out << '\n';
  }

  if (sparta->particle->nmixture <= 0) {
    out << "#   mixtures = none\n";
  } else {
    out << "#   mixtures = " << sparta->particle->nmixture << '\n';
    for (int i = 0; i < sparta->particle->nmixture; ++i) {
      Mixture *mixture = sparta->particle->mixture[i];
      out << "#     " << mixture->id << " temp " << mixture->temp_thermal
          << " K vstream [" << mixture->vstream[0] << ", " << mixture->vstream[1]
          << ", " << mixture->vstream[2] << "] m/s";
      if (mixture->nspecies > 0) {
        out << " fractions";
        for (int j = 0; j < mixture->nspecies; ++j) {
          const int species = mixture->species[j];
          out << " " << sparta->particle->species[species].id << "=" << mixture->fraction[j];
        }
      }
      out << '\n';
    }
  }

  if (sparta->surf->nsc <= 0) {
    out << "#   surface collision models = none\n";
  } else {
    out << "#   surface collision models = " << sparta->surf->nsc << '\n';
    for (int i = 0; i < sparta->surf->nsc; ++i) {
      out << "#     " << sparta->surf->sc[i]->id << " style " << sparta->surf->sc[i]->style
          << '\n';
    }
  }
  if (sparta->surf->nsr <= 0) {
    out << "#   surface reaction models = none\n";
  } else {
    out << "#   surface reaction models = " << sparta->surf->nsr << '\n';
    for (int i = 0; i < sparta->surf->nsr; ++i) {
      SurfReact *reaction = sparta->surf->sr[i];
      out << "#     " << reaction->id << " style " << reaction->style << '\n';
      for (int j = 0; j < reaction->nlist; ++j) {
        out << "#       " << reaction->reactionID(j) << '\n';
      }
    }
  }

  if (grid_vtu_dumps.empty()) {
    out << "#   fluid dumps = none registered\n";
  } else {
    out << "#   fluid dumps = " << grid_vtu_dumps.size() << '\n';
    for (const auto &dump : grid_vtu_dumps) {
      out << "#     fix " << dump.fix_id << " path " << dump.path << " index "
          << dump.index_mode << " fields " << join_strings(dump.fields, ", ") << '\n';
    }
  }
  out << "# end DSMC/SPARTA configuration\n";
}

void print_coupled_summary_if_needed(SPARTA *sparta) {
  if (!owns_model(sparta) || stats_header_printed) {
    return;
  }
  auto &m = model(sparta);
  std::ostringstream out;
  m.print_run_summary_public(out, "Coupled voxel/ISTHMUS/DSMC ablation");
  write_to_dsmc_outputs(sparta, prefixed_lines(out.str(), "[IAC] ", true));
  out.str("");
  out.clear();
  print_dsmc_summary(sparta, out);
  write_to_dsmc_outputs(sparta, prefixed_lines(out.str(), "[SPA] ", true));
  stats_header_printed = true;
}

void print_stats_after_step(SPARTA *sparta) {
  if (!owns_model(sparta)) {
    return;
  }
  auto &m = model(sparta);
  const int every = config().stats.every;
  if (every <= 0 || m.step_count() % every != 0) {
    return;
  }
  std::ostringstream out;
  print_coupled_summary_if_needed(sparta);
  m.print_stats_header(out);
  m.print_latest_stats(out);
  write_to_dsmc_outputs(sparta, prefixed_stats_table(out.str(), "[IAC] ", true, 5));
  last_compact_output = OUTPUT_IAC;
}

void print_sparta_stats(SPARTA *sparta) {
  print_coupled_summary_if_needed(sparta);

  FILE *capture = nullptr;
  FILE *old_screen = nullptr;
  FILE *old_logfile = nullptr;
  if (sparta->comm->me == 0) {
    capture = std::tmpfile();
    if (!capture) {
      sparta->error->all(FLERR, "Could not create temporary stream for iac_spa_stats");
    }
    old_screen = sparta->screen;
    old_logfile = sparta->logfile;
    sparta->screen = capture;
    sparta->logfile = nullptr;
  }

  sparta->output->stats->header();
  sparta->output->stats->compute(1);

  if (sparta->comm->me == 0) {
    std::fflush(capture);
    const std::string captured = read_stream(capture);
    std::fclose(capture);
    sparta->screen = old_screen;
    sparta->logfile = old_logfile;
    const bool include_header = last_compact_output != OUTPUT_SPA;
    write_to_dsmc_outputs(sparta,
                          prefixed_stats_table(captured, "[SPA] ", false, 12, include_header));
    last_compact_output = OUTPUT_SPA;
  }
}

void generate_surface(SPARTA *sparta, const iac::IsthmusSurfaceCommand &surface) {
  if (owns_model(sparta)) {
    auto &m = model(sparta);
    m.generate_surface(surface);
    surface_cache[surface.name] = m.surface_triangles(surface.name);
  }
  broadcast_surface_cache(sparta, surface.name);
}

const std::vector<iac::Model::PublicSurfaceTriangle> &
surface_triangles(SPARTA *sparta, const std::string &surface_id) {
  (void)sparta;
  const auto found = surface_cache.find(surface_id);
  if (found == surface_cache.end()) {
    throw std::runtime_error("IAC surface '" + surface_id + "' has not been generated");
  }
  return found->second;
}

void install_surface(SPARTA *sparta, const char *surface_id, int partflag, int type) {
  require_grid(sparta);
  Surf *surf = sparta->surf;
  Grid *grid = sparta->grid;
  Particle *particle = sparta->particle;
  Comm *comm = sparta->comm;
  Error *error = sparta->error;
  Memory *memory = sparta->memory;
  MPI_Comm world = sparta->world;

  if (surf->exist && surf->nsurf > 0) {
    error->all(FLERR, "surface install requires removing existing surfaces first");
  }

  const auto &triangles = surface_triangles(sparta, surface_id);
  if (triangles.empty()) {
    error->all(FLERR, "surface install found no triangles for requested surface");
  }
  if (sparta->domain->dimension != 3) {
    error->all(FLERR, "surface install currently supports 3d triangle surfaces only");
  }

  const std::size_t total_triangles = triangles.size();
  const std::size_t base = total_triangles / static_cast<std::size_t>(comm->nprocs);
  const std::size_t extra = total_triangles % static_cast<std::size_t>(comm->nprocs);
  const std::size_t first =
      static_cast<std::size_t>(comm->me) * base +
      std::min(static_cast<std::size_t>(comm->me), extra);
  const std::size_t count = base + (static_cast<std::size_t>(comm->me) < extra ? 1 : 0);

  Surf::Tri *tris = count > 0
                        ? static_cast<Surf::Tri *>(memory->smalloc(
                              count * sizeof(Surf::Tri), "isthmus_ablation:tris"))
                        : nullptr;
  for (std::size_t local = 0; local < count; ++local) {
    const std::size_t i = first + local;
    const auto &src = triangles[i];
    Surf::Tri &tri = tris[local];
    tri.id = static_cast<surfint>(i + 1);
    tri.type = type;
    tri.mask = 1;
    tri.isc = tri.isr = -1;
    std::memcpy(tri.p1, src.a.data(), 3 * sizeof(double));
    std::memcpy(tri.p2, src.b.data(), 3 * sizeof(double));
    std::memcpy(tri.p3, src.c.data(), 3 * sizeof(double));
    tri.norm[0] = tri.norm[1] = tri.norm[2] = 0.0;
    tri.transparent = 0;
  }

  const bigint nsurf_old = surf->nsurf;
  const int nsurf_old_mine = surf->distributed ? surf->nown : surf->nlocal;
  surf->exist = 1;
  surf->add_surfs(0, static_cast<int>(count), nullptr, tris, 0, nullptr, nullptr);
  if (tris) {
    memory->sfree(tris);
  }

  surf->output_extent(nsurf_old_mine);
  surf->compute_tri_normal(nsurf_old_mine);
  surf->check_point_inside(nsurf_old_mine);
  surf->check_watertight_3d();

  if (particle->exist) {
    particle->sort();
  }

  surf->setup_owned();
  grid->unset_neighbors();
  grid->remove_ghosts();

  if (particle->exist && grid->nsplitlocal) {
    Grid::ChildCell *cells = grid->cells;
    const int nglocal = grid->nlocal;
    for (int icell = 0; icell < nglocal; icell++) {
      if (cells[icell].nsplit > 1) {
        grid->combine_split_cell_particles(icell, 1);
      }
    }
  }

  grid->clear_surf();
  grid->surf2grid(1, 0);
  surf->check_point_near_surf_3d();

  grid->setup_owned();
  grid->acquire_ghosts();
  grid->reset_neighbors();
  comm->reset_neighbors();
  grid->set_inout();
  grid->type_check();

  if (particle->exist) {
    Grid::ChildCell *cells = grid->cells;
    Grid::ChildInfo *cinfo = grid->cinfo;
    const int nglocal = grid->nlocal;
    int delflag = 0;

    for (int icell = 0; icell < nglocal; icell++) {
      if (cinfo[icell].type == CELL_INSIDE) {
        if (partflag == PART_KEEP && cinfo[icell].count > 0) {
          error->one(FLERR, "Particles are inside new surfaces");
        }
        if (cinfo[icell].count) {
          delflag = 1;
        }
        particle->remove_all_from_cell(cinfo[icell].first);
        cinfo[icell].count = 0;
        cinfo[icell].first = -1;
        continue;
      }
      if (cells[icell].nsurf && cells[icell].nsplit >= 1) {
        const int nsurf = cells[icell].nsurf;
        surfint *csurfs = cells[icell].csurfs;
        int m;
        for (m = 0; m < nsurf; m++) {
          if (surf->tris[csurfs[m]].id >= nsurf_old) {
            break;
          }
        }
        if (m < nsurf && partflag == PART_CHECK) {
          if (cinfo[icell].count) {
            delflag = 1;
          }
          particle->remove_all_from_cell(cinfo[icell].first);
          cinfo[icell].count = 0;
          cinfo[icell].first = -1;
        }
      }
      if (cells[icell].nsplit > 1) {
        grid->assign_split_cell_particles(icell);
      }
    }

    if (delflag) {
      particle->compress_rebalance();
    }
  }

  MPI_Barrier(world);
}

} // namespace IACBridge
} // namespace SPARTA_NS
