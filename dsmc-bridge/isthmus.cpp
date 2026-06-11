#include "isthmus.h"

#include "error.h"
#include "iacbridge.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

using namespace SPARTA_NS;

namespace {

bool parse_bool(const char *value) {
  return std::strcmp(value, "yes") == 0 || std::strcmp(value, "true") == 0 ||
         std::strcmp(value, "1") == 0;
}

double parse_positive_double(const char *value, Error *error, const char *context) {
  char *end = nullptr;
  const double parsed = std::strtod(value, &end);
  if (end == value || *end != '\0' || parsed <= 0.0) {
    error->all(FLERR, context);
  }
  return parsed;
}

double parse_ratio(const char *value, Error *error) {
  if (std::strcmp(value, "voxel") == 0) {
    return 1.0;
  }
  const char *colon = std::strchr(value, ':');
  if (!colon) {
    return parse_positive_double(value, error,
                                 "isthmus surface resolution must be positive");
  }

  const std::string numerator(value, colon - value);
  const std::string denominator(colon + 1);
  const double top = parse_positive_double(
      numerator.c_str(), error, "isthmus surface resolution ratio is invalid");
  const double bottom = parse_positive_double(
      denominator.c_str(), error, "isthmus surface resolution ratio is invalid");
  return top / bottom;
}

const char *value_after(int narg, char **arg, const char *key) {
  for (int i = 0; i + 1 < narg; ++i) {
    if (std::strcmp(arg[i], key) == 0) {
      return arg[i + 1];
    }
  }
  return nullptr;
}

void forward_isthmus(SPARTA *sparta, const char *subcommand, int narg, char **arg) {
  std::vector<char *> forwarded;
  forwarded.reserve(static_cast<std::size_t>(narg) + 1);
  forwarded.push_back(const_cast<char *>(subcommand));
  for (int i = 0; i < narg; ++i) {
    forwarded.push_back(arg[i]);
  }
  Isthmus isthmus(sparta);
  isthmus.command(static_cast<int>(forwarded.size()), forwarded.data());
}

} // namespace

Isthmus::Isthmus(SPARTA *sparta) : Pointers(sparta) {}

IsthmusSurface::IsthmusSurface(SPARTA *sparta) : Pointers(sparta) {}
void IsthmusSurface::command(int narg, char **arg) {
  forward_isthmus(sparta, "surface", narg, arg);
}

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
  const char *resolution = value_after(narg - 2, arg + 2, "resolution");
  if (buffer) {
    surface.buffer = std::atoi(buffer);
  }
  if (resolution) {
    surface.resolution = parse_ratio(resolution, error);
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
    IACBridge::generate_surface(sparta, surface);
  } catch (const std::exception &ex) {
    error->all(FLERR, ex.what());
  }
}
