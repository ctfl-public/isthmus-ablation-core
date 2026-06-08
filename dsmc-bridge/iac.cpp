#include "iac.h"

#include "error.h"
#include "iacbridge.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

using namespace SPARTA_NS;

Iac::Iac(SPARTA *sparta) : Pointers(sparta) {}

void Iac::command(int narg, char **arg) {
  if (narg < 1) {
    error->all(FLERR, "Illegal iac command");
  }

  if (std::strcmp(arg[0], "verify") == 0) {
    if (narg < 6 || std::strcmp(arg[2], "exact") != 0 ||
        std::strcmp(arg[4], "tolerance") != 0) {
      error->all(FLERR, "Illegal iac verify command");
    }
    iac::VerificationCheck check;
    check.quantity = arg[1];
    check.expression = arg[3];
    check.tolerance = std::atof(arg[5]);
    for (int i = 6; i < narg; ++i) {
      if (std::strcmp(arg[i], "percent") == 0) {
        check.tolerance_mode = "percent";
      } else if (std::strcmp(arg[i], "absolute") == 0) {
        check.tolerance_mode = "absolute";
      } else if (std::strcmp(arg[i], "norm") == 0) {
        if (i + 1 >= narg) {
          error->all(FLERR, "iac verify norm is missing a value");
        }
        check.norm = arg[++i];
      } else {
        error->all(FLERR, "Unknown iac verify option");
      }
    }
    try {
      auto &model = IACBridge::model(sparta);
      if (model.has_diagnostic(check.quantity)) {
        model.verify_diagnostic(check);
      } else {
        const double error_value = model.verification_error(check);
        if (!(error_value <= check.tolerance)) {
          std::string message = "iac verify failed for '" + check.quantity +
                                "': error " + std::to_string(error_value) +
                                " exceeds tolerance " +
                                std::to_string(check.tolerance);
          if (check.tolerance_mode == "percent") {
            message += " percent";
          }
          error->all(FLERR, message.c_str());
        }
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "timestep") == 0) {
    if (narg < 2) {
      error->all(FLERR, "Illegal iac timestep command");
    }
    auto &cfg = IACBridge::config();
    try {
      if (std::strcmp(arg[1], "mass/courant") == 0) {
        if (narg != 5 || std::strcmp(arg[3], "source") != 0) {
          error->all(FLERR, "Expected iac timestep mass/courant <value> source <name>");
        }
        cfg.timestep.kind = iac::TimestepKind::MassCourant;
        cfg.timestep.courant = std::atof(arg[2]);
        cfg.timestep.source = arg[4];
        IACBridge::model(sparta).set_timestep_from_source_courant(cfg.timestep.courant,
                                                                  cfg.timestep.source);
      } else {
        cfg.timestep.kind = iac::TimestepKind::Explicit;
        cfg.timestep.value = std::atof(arg[1]);
        IACBridge::model(sparta).set_timestep(cfg.timestep.value);
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "stats") == 0) {
    if (narg != 2) {
      error->all(FLERR, "Expected iac stats <N>");
    }
    auto &cfg = IACBridge::config();
    cfg.stats.every = std::atoi(arg[1]);
    if (cfg.stats.every <= 0) {
      error->all(FLERR, "iac stats interval must be positive");
    }
    IACBridge::reset_stats_output();
    return;
  }

  if (std::strcmp(arg[0], "stats-style") == 0) {
    if (narg < 2) {
      error->all(FLERR, "Expected iac stats-style <column> ...");
    }
    auto &columns = IACBridge::config().stats.columns;
    columns.clear();
    for (int i = 1; i < narg; ++i) {
      columns.push_back(arg[i]);
    }
    IACBridge::reset_stats_output();
    return;
  }

  if (std::strcmp(arg[0], "print") == 0) {
    if (narg != 2) {
      error->all(FLERR, "Illegal iac print command");
    }
    try {
      const double value = IACBridge::model(sparta).diagnostic(arg[1]);
      if (screen) {
        std::fprintf(screen, "IAC diagnostic %s = %.17g\n", arg[1], value);
      }
      if (logfile) {
        std::fprintf(logfile, "IAC diagnostic %s = %.17g\n", arg[1], value);
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  error->all(FLERR, "Illegal iac command");
}
