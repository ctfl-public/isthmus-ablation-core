#include "voxel.h"

#include "error.h"
#include "iacbridge.h"
#include "update.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

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

} // namespace

Voxel::Voxel(SPARTA *sparta) : Pointers(sparta) {}

void Voxel::command(int narg, char **arg) {
  if (narg < 1) {
    error->all(FLERR, "Illegal voxel command");
  }

  auto &cfg = IACBridge::config();

  if (std::strcmp(arg[0], "material") == 0) {
    if (narg != 4 || std::strcmp(arg[2], "density") != 0) {
      error->all(FLERR, "Illegal voxel material command");
    }
    cfg.material.name = arg[1];
    cfg.material.density = std::atof(arg[3]);
    IACBridge::reset_model();
    return;
  }

  if (std::strcmp(arg[0], "create") == 0) {
    if (narg < 4 || std::strcmp(arg[2], "sphere") != 0) {
      error->all(FLERR, "Only voxel create <id> sphere ... is currently supported");
    }
    cfg.voxel_name = arg[1];
    cfg.geometry = iac::GeometryKind::Sphere;
    const char *diameter = value_after(narg - 3, arg + 3, "diameter");
    const char *resolution = value_after(narg - 3, arg + 3, "resolution");
    const char *material = value_after(narg - 3, arg + 3, "material");
    if (!diameter || !resolution || !material) {
      error->all(FLERR, "voxel create sphere requires diameter, resolution, and material");
    }
    cfg.sphere.diameter = std::atof(diameter);
    cfg.sphere.resolution = std::atoi(resolution);
    cfg.sphere.dx = cfg.sphere.diameter / static_cast<double>(cfg.sphere.resolution);
    cfg.sphere.material = material;
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
    const char *policy = value_after(narg - 2, arg + 2, "policy");
    const char *delete_empty = value_after(narg - 2, arg + 2, "delete");
    if (!surface || !policy) {
      error->all(FLERR, "voxel ablate requires surface and policy");
    }
    ablate.surface = surface;
    ablate.policy = policy;
    if (delete_empty) {
      ablate.delete_empty = parse_bool(delete_empty);
    }
    try {
      IACBridge::model(sparta).ablate(ablate);
      IACBridge::model(sparta).advance_steps(1);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
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
      IACBridge::model(sparta).write_history(arg[2]);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  error->all(FLERR, "Illegal voxel command");
}
