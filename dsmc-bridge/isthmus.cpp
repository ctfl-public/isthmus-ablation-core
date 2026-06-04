#include "isthmus.h"

#include "error.h"
#include "iacbridge.h"

#include <cstdlib>
#include <cstring>
#include <exception>

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

Isthmus::Isthmus(SPARTA *sparta) : Pointers(sparta) {}

void Isthmus::command(int narg, char **arg) {
  if (narg < 4 || std::strcmp(arg[0], "surface") != 0) {
    error->all(FLERR, "Illegal isthmus command");
  }

  iac::IsthmusSurfaceCommand surface;
  surface.name = arg[1];
  const char *voxels = value_after(narg - 2, arg + 2, "voxels");
  if (!voxels) {
    error->all(FLERR, "isthmus surface requires voxels");
  }
  surface.voxels = voxels;

  const char *buffer = value_after(narg - 2, arg + 2, "buffer");
  const char *weighting = value_after(narg - 2, arg + 2, "weighting");
  const char *map = value_after(narg - 2, arg + 2, "map");
  const char *crop = value_after(narg - 2, arg + 2, "crop");
  if (buffer) {
    surface.buffer = std::atoi(buffer);
  }
  if (weighting) {
    surface.weighting = parse_bool(weighting);
  }
  if (map) {
    surface.map = parse_bool(map);
  }
  if (crop && std::strcmp(crop, "real") == 0) {
    surface.crop_real = true;
  }

  try {
    IACBridge::model(sparta).generate_surface(surface);
  } catch (const std::exception &ex) {
    error->all(FLERR, ex.what());
  }
}
