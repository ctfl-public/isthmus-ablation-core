#include "voxel.h"

#include "comm.h"
#include "error.h"
#include "iacbridge.h"
#include "update.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

using namespace SPARTA_NS;

namespace {

bool parse_bool(const char *value) {
  return std::strcmp(value, "yes") == 0 || std::strcmp(value, "true") == 0 ||
         std::strcmp(value, "1") == 0;
}

const char *value_after(int narg, char **arg, const char *key) {
  for (int i = 0; i + 1 < narg; ++i) {
    if (std::strcmp(arg[i], key) == 0) {
      return arg[i + 1];
    }
  }
  return nullptr;
}

void forward_voxel(SPARTA *sparta, const char *subcommand, int narg, char **arg) {
  std::vector<char *> forwarded;
  forwarded.reserve(static_cast<std::size_t>(narg) + 1);
  forwarded.push_back(const_cast<char *>(subcommand));
  for (int i = 0; i < narg; ++i) {
    forwarded.push_back(arg[i]);
  }
  Voxel voxel(sparta);
  voxel.command(static_cast<int>(forwarded.size()), forwarded.data());
}

} // namespace

Voxel::Voxel(SPARTA *sparta) : Pointers(sparta) {}

VoxelAblate::VoxelAblate(SPARTA *sparta) : Pointers(sparta) {}
void VoxelAblate::command(int narg, char **arg) { forward_voxel(sparta, "ablate", narg, arg); }

VoxelCreate::VoxelCreate(SPARTA *sparta) : Pointers(sparta) {}
void VoxelCreate::command(int narg, char **arg) { forward_voxel(sparta, "create", narg, arg); }

VoxelDumpCommand::VoxelDumpCommand(SPARTA *sparta) : Pointers(sparta) {}
void VoxelDumpCommand::command(int narg, char **arg) { forward_voxel(sparta, "dump", narg, arg); }

VoxelGhost::VoxelGhost(SPARTA *sparta) : Pointers(sparta) {}
void VoxelGhost::command(int narg, char **arg) { forward_voxel(sparta, "ghost", narg, arg); }

VoxelMaterial::VoxelMaterial(SPARTA *sparta) : Pointers(sparta) {}
void VoxelMaterial::command(int narg, char **arg) { forward_voxel(sparta, "material", narg, arg); }

VoxelWriteHistory::VoxelWriteHistory(SPARTA *sparta) : Pointers(sparta) {}
void VoxelWriteHistory::command(int narg, char **arg) {
  forward_voxel(sparta, "write-history", narg, arg);
}

VoxelWriteVtu::VoxelWriteVtu(SPARTA *sparta) : Pointers(sparta) {}
void VoxelWriteVtu::command(int narg, char **arg) { forward_voxel(sparta, "write-vtu", narg, arg); }

void Voxel::command(int narg, char **arg) {
  if (narg < 1) {
    error->all(FLERR, "Illegal voxel command");
  }

  auto &cfg = IACBridge::config();

  if (std::strcmp(arg[0], "material") == 0) {
    if (narg < 4) {
      error->all(FLERR, "voxel material requires density");
    }
    const char *density = value_after(narg - 2, arg + 2, "density");
    if (!density) {
      error->all(FLERR, "voxel material requires density");
    }
    iac::Material material;
    material.name = arg[1];
    material.density = std::atof(density);
    const char *id = value_after(narg - 2, arg + 2, "id");
    material.id = id ? std::atoi(id) : static_cast<int>(cfg.materials.size()) + 1;
    const char *molar_mass = value_after(narg - 2, arg + 2, "molar-mass");
    const char *formula = value_after(narg - 2, arg + 2, "formula");
    if (molar_mass) {
      material.molar_mass = std::atof(molar_mass);
    }
    if (formula) {
      material.formula = formula;
    }
    auto existing = std::find_if(cfg.materials.begin(), cfg.materials.end(),
                                 [&](const iac::Material &entry) {
                                   return entry.name == material.name || entry.id == material.id;
                                 });
    if (existing != cfg.materials.end()) {
      *existing = material;
    } else {
      cfg.materials.push_back(material);
    }
    if (cfg.material.name.empty()) {
      cfg.material = material;
    }
    IACBridge::reset_model();
    return;
  }

  if (std::strcmp(arg[0], "create") == 0) {
    if (narg < 4) {
      error->all(FLERR, "Illegal voxel create command");
    }
    cfg.voxel_name = arg[1];
    iac::VoxelCreate create;
    create.voxel_name = arg[1];
    if (std::strcmp(arg[2], "sphere") == 0) {
      create.geometry = iac::GeometryKind::Sphere;
      const char *diameter = value_after(narg - 3, arg + 3, "diameter");
      const char *resolution = value_after(narg - 3, arg + 3, "resolution");
      const char *material = value_after(narg - 3, arg + 3, "material");
      if (!diameter || !resolution || !material) {
        error->all(FLERR, "voxel create sphere requires diameter, resolution, and material");
      }
      create.sphere.diameter = std::atof(diameter);
      create.sphere.resolution = std::atoi(resolution);
      create.sphere.dx = create.sphere.diameter / static_cast<double>(create.sphere.resolution);
      create.sphere.material = material;
      const char *cx = value_after(narg - 3, arg + 3, "cx");
      const char *cy = value_after(narg - 3, arg + 3, "cy");
      const char *cz = value_after(narg - 3, arg + 3, "cz");
      if (cx) create.sphere.center[0] = std::atof(cx);
      if (cy) create.sphere.center[1] = std::atof(cy);
      if (cz) create.sphere.center[2] = std::atof(cz);
      cfg.geometry = iac::GeometryKind::Sphere;
      cfg.sphere = create.sphere;
    } else if (std::strcmp(arg[2], "slab") == 0) {
      create.geometry = iac::GeometryKind::Slab;
      const char *nx = value_after(narg - 3, arg + 3, "nx");
      const char *ny = value_after(narg - 3, arg + 3, "ny");
      const char *nz = value_after(narg - 3, arg + 3, "nz");
      const char *dx = value_after(narg - 3, arg + 3, "dx");
      const char *material = value_after(narg - 3, arg + 3, "material");
      if (!nx || !ny || !nz || !dx || !material) {
        error->all(FLERR, "voxel create slab requires nx, ny, nz, dx, and material");
      }
      create.slab.nx = std::atoi(nx);
      create.slab.ny = std::atoi(ny);
      create.slab.nz = std::atoi(nz);
      create.slab.dx = std::atof(dx);
      create.slab.material = material;
      const char *ox = value_after(narg - 3, arg + 3, "ox");
      const char *oy = value_after(narg - 3, arg + 3, "oy");
      const char *oz = value_after(narg - 3, arg + 3, "oz");
      if (ox) create.slab.origin[0] = std::atof(ox);
      if (oy) create.slab.origin[1] = std::atof(oy);
      if (oz) create.slab.origin[2] = std::atof(oz);
      cfg.geometry = iac::GeometryKind::Slab;
      cfg.slab = create.slab;
    } else if (std::strcmp(arg[2], "tiff") == 0) {
      create.geometry = iac::GeometryKind::Tiff;
      const char *file = value_after(narg - 3, arg + 3, "file");
      const char *dx = value_after(narg - 3, arg + 3, "dx");
      const char *material = value_after(narg - 3, arg + 3, "material");
      const char *materials = value_after(narg - 3, arg + 3, "materials");
      const char *ox = value_after(narg - 3, arg + 3, "ox");
      const char *oy = value_after(narg - 3, arg + 3, "oy");
      const char *oz = value_after(narg - 3, arg + 3, "oz");
      const char *axes = value_after(narg - 3, arg + 3, "axes");
      const char *origin = value_after(narg - 3, arg + 3, "origin");
      const char *origin_mode = value_after(narg - 3, arg + 3, "origin-mode");
      if (!file || !dx || (!material && !materials)) {
        error->all(FLERR, "voxel create tiff requires file, dx, and material or materials labels");
      }
      if (materials && std::strcmp(materials, "labels") != 0) {
        error->all(FLERR, "voxel create tiff materials must be labels");
      }
      if (origin && origin_mode) {
        error->all(FLERR, "voxel create tiff accepts either origin or origin-mode, not both");
      }
      create.tiff.file = file;
      create.tiff.dx = std::atof(dx);
      if (material) {
        create.tiff.material = material;
      }
      if (materials) {
        create.tiff.material_labels = true;
      }
      if (ox) {
        create.tiff.origin[0] = std::atof(ox);
      }
      if (oy) {
        create.tiff.origin[1] = std::atof(oy);
      }
      if (oz) {
        create.tiff.origin[2] = std::atof(oz);
      }
      if (axes) {
        create.tiff.axes = axes;
      }
      if (origin) {
        create.tiff.origin_mode = origin;
      }
      if (origin_mode) {
        create.tiff.origin_mode = origin_mode;
      }
      cfg.geometry = iac::GeometryKind::Tiff;
      cfg.tiff = create.tiff;
    } else {
      error->all(FLERR, "voxel create style must be slab, sphere, or tiff");
    }
    cfg.creates.push_back(std::move(create));
    cfg.timestep.kind = iac::TimestepKind::Explicit;
    cfg.timestep.value = update->dt > 0.0 ? update->dt : 1.0;
    IACBridge::reset_model();
    return;
  }

  if (std::strcmp(arg[0], "ablate") == 0) {
    if (narg < 6) {
      error->all(FLERR, "Illegal voxel ablate command");
    }
    iac::AblationCommand ablate;
    ablate.voxels = arg[1];
    const char *surface = value_after(narg - 2, arg + 2, "surface");
    const char *source = value_after(narg - 2, arg + 2, "source");
    const char *policy = value_after(narg - 2, arg + 2, "policy");
    const char *face = value_after(narg - 2, arg + 2, "face");
    const char *delete_empty = value_after(narg - 2, arg + 2, "delete");
    if ((!surface && !source) || !policy) {
      error->all(FLERR, "voxel ablate requires source or surface, and policy");
    }
    if (source && !surface && !face) {
      error->all(FLERR, "voxel ablate source mode requires face <xlo|xhi|ylo|yhi|zlo|zhi>");
    }
    if (surface && face) {
      error->all(FLERR, "voxel ablate face is only valid for source mode; use surface flux selection for surface mode");
    }
    if (surface) {
      ablate.surface = surface;
    }
    if (source) {
      ablate.source = source;
    }
    ablate.policy = policy;
    if (face) {
      ablate.face = face;
    }
    if (delete_empty) {
      ablate.delete_empty = parse_bool(delete_empty);
    }
    std::string root_error;
    try {
      if (IACBridge::owns_model(sparta)) {
        IACBridge::model(sparta).ablate(ablate);
      }
    } catch (const std::exception &ex) {
      root_error = ex.what();
    }
    IACBridge::error_if_root_failed(sparta, root_error);
    return;
  }

  if (std::strcmp(arg[0], "dump") == 0) {
    if (narg == 2 && std::strcmp(arg[1], "off") == 0) {
      cfg.dumps.clear();
      return;
    }
    if (narg < 6) {
      error->all(FLERR, "Illegal voxel dump command");
    }
    iac::VoxelDump dump;
    dump.id = arg[1];
    dump.voxels = arg[2];
    dump.style = arg[3];
    dump.every = std::atoi(arg[4]);
    dump.path = arg[5];
    if (dump.every <= 0) {
      error->all(FLERR, "voxel dump interval must be positive");
    }
    if (dump.style != "history" && dump.style != "vtu") {
      error->all(FLERR, "voxel dump style must be history or vtu");
    }
    const char *select_value = value_after(narg - 6, arg + 6, "select");
    const char *scalar_value = value_after(narg - 6, arg + 6, "scalar");
    if (select_value) {
      dump.select = select_value;
    }
    if (scalar_value) {
      dump.scalar = scalar_value;
    }
    cfg.dumps.push_back(std::move(dump));
    IACBridge::reset_model();
    return;
  }

  if (std::strcmp(arg[0], "ghost") == 0) {
    if (narg < 7) {
      error->all(FLERR, "Illegal voxel ghost command");
    }
    iac::VoxelGhostCommand ghost;
    ghost.voxels = arg[1];
    const char *axis = value_after(narg - 2, arg + 2, "axis");
    const char *side = value_after(narg - 2, arg + 2, "side");
    const char *boundary = value_after(narg - 2, arg + 2, "boundary");
    const char *layers = value_after(narg - 2, arg + 2, "layers");
    if (!axis || !boundary) {
      error->all(FLERR, "voxel ghost requires axis and boundary");
    }
    ghost.axis = axis;
    if (side) {
      ghost.side = side;
    }
    ghost.boundary = boundary;
    if (layers) {
      ghost.layers = std::atoi(layers);
    }
    cfg.ghosts.push_back(std::move(ghost));
    IACBridge::reset_model();
    return;
  }

  if (std::strcmp(arg[0], "write-history") == 0) {
    if (narg != 3) {
      error->all(FLERR, "Illegal voxel write-history command");
    }
    if (std::strcmp(arg[1], cfg.voxel_name.c_str()) != 0) {
      error->all(FLERR, "voxel write-history references unknown voxel model");
    }
    try {
      if (IACBridge::owns_model(sparta)) {
        IACBridge::model(sparta).write_history(arg[2]);
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "write-vtu") == 0) {
    if (narg < 3) {
      error->all(FLERR, "Illegal voxel write-vtu command");
    }
    if (std::strcmp(arg[1], cfg.voxel_name.c_str()) != 0) {
      error->all(FLERR, "voxel write-vtu references unknown voxel model");
    }
    std::string select = "active";
    std::string scalar = "mf";
    const char *select_value = value_after(narg - 3, arg + 3, "select");
    const char *scalar_value = value_after(narg - 3, arg + 3, "scalar");
    if (select_value) {
      select = select_value;
    }
    if (scalar_value) {
      scalar = scalar_value;
    }
    try {
      if (IACBridge::owns_model(sparta)) {
        IACBridge::model(sparta).write_voxels_vtu(arg[2], select, scalar);
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  error->all(FLERR, "Illegal voxel command");
}
