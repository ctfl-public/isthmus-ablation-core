#include "source.h"

#include "error.h"
#include "iacbridge.h"

#include <cstdlib>
#include <cstring>

using namespace SPARTA_NS;

namespace {

const char *value_after(int narg, char **arg, const char *key) {
  for (int i = 0; i + 1 < narg; ++i) {
    if (std::strcmp(arg[i], key) == 0) {
      return arg[i + 1];
    }
  }
  return nullptr;
}

} // namespace

Source::Source(SPARTA *sparta) : Pointers(sparta) {}

void Source::command(int narg, char **arg) {
  if (narg < 3 || std::strcmp(arg[1], "constant") != 0) {
    error->all(FLERR, "Expected source <id> constant <value> [units <units>]");
  }
  auto &cfg = IACBridge::config();
  cfg.source.name = arg[0];
  cfg.source.value = std::atof(arg[2]);
  const char *units = value_after(narg - 3, arg + 3, "units");
  if (units) {
    cfg.source.units = units;
  }
  IACBridge::reset_model();
}
