#include "iacbridge.h"

#include "comm.h"
#include "domain.h"
#include "error.h"
#include "grid.h"
#include "memory.h"
#include "particle.h"
#include "sparta.h"
#include "surf.h"
#include "update.h"

#include <cstring>
#include <cstdio>
#include <memory>
#include <sstream>

namespace SPARTA_NS {
namespace IACBridge {
namespace {

enum { PART_NONE, PART_CHECK, PART_KEEP };
enum { CELL_UNKNOWN, CELL_OUTSIDE, CELL_INSIDE, CELL_OVERLAP };

std::unique_ptr<iac::Model> active_model;
iac::Config active_config;
bigint last_coupling_step = 0;
bool stats_header_printed = false;

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

} // namespace

iac::Config &config() {
  static bool initialized = false;
  if (!initialized) {
    initialize_defaults();
    initialized = true;
  }
  return active_config;
}

iac::Model &model(SPARTA *sparta) {
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
  reset_stats_output();
}

void set_coupling_interval_from_dsmc(SPARTA *sparta) {
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
  model(sparta);
  last_coupling_step = sparta->update->ntimestep;
}

void reset_stats_output() {
  stats_header_printed = false;
}

void write_to_dsmc_outputs(SPARTA *sparta, const std::string &text) {
  if (sparta->screen) {
    std::fprintf(sparta->screen, "%s", text.c_str());
  }
  if (sparta->logfile) {
    std::fprintf(sparta->logfile, "%s", text.c_str());
  }
}

void print_stats_after_step(SPARTA *sparta) {
  auto &m = model(sparta);
  const int every = config().stats.every;
  if (every <= 0 || m.step_count() % every != 0) {
    return;
  }
  std::ostringstream out;
  if (!stats_header_printed) {
    m.print_run_summary_public(out);
    m.print_stats_header(out);
    stats_header_printed = true;
  }
  m.print_latest_stats(out);
  write_to_dsmc_outputs(sparta, out.str());
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

  const auto triangles = model(sparta).surface_triangles(surface_id);
  if (triangles.empty()) {
    error->all(FLERR, "surface install found no triangles for requested surface");
  }
  if (sparta->domain->dimension != 3) {
    error->all(FLERR, "surface install currently supports 3d triangle surfaces only");
  }

  Surf::Tri *tris =
      static_cast<Surf::Tri *>(memory->smalloc(triangles.size() * sizeof(Surf::Tri),
                                               "isthmus_ablation:tris"));
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    const auto &src = triangles[i];
    Surf::Tri &tri = tris[i];
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
  surf->add_surfs(0, static_cast<int>(triangles.size()), nullptr, tris, 0, nullptr, nullptr);
  memory->sfree(tris);

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
