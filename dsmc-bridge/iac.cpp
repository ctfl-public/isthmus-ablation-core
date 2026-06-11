#include "iac.h"

#include "comm.h"
#include "error.h"
#include "iacbridge.h"
#include "input.h"
#include "variable.h"

#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

using namespace SPARTA_NS;

namespace {

void set_internal_variable(Input *input, Error *error, const char *name, double value) {
  Variable *variable = input->variable;
  int ivar = variable->find(const_cast<char *>(name));
  if (ivar < 0) {
    variable->internal_create(const_cast<char *>(name), value);
    return;
  }
  if (!variable->internal_style(ivar)) {
    std::string message = "IAC variable '" + std::string(name) + "' must be internal style";
    error->all(FLERR, message.c_str());
  }
  variable->internal_set(ivar, value);
}

double broadcast_root_value(SPARTA *sparta, double value) {
  MPI_Bcast(&value, 1, MPI_DOUBLE, 0, sparta->world);
  return value;
}

} // namespace

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
      if (IACBridge::owns_model(sparta)) {
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
        if (IACBridge::owns_model(sparta)) {
          IACBridge::model(sparta).set_timestep_from_source_courant(cfg.timestep.courant,
                                                                    cfg.timestep.source);
        }
      } else {
        cfg.timestep.kind = iac::TimestepKind::Explicit;
        cfg.timestep.value = std::atof(arg[1]);
        if (IACBridge::owns_model(sparta)) {
          IACBridge::model(sparta).set_timestep(cfg.timestep.value);
        }
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

  if (std::strcmp(arg[0], "stats_style") == 0 || std::strcmp(arg[0], "stats-style") == 0) {
    if (narg < 2) {
      error->all(FLERR, "Expected iac stats_style <column> ...");
    }
    auto &columns = IACBridge::config().stats.columns;
    columns.clear();
    for (int i = 1; i < narg; ++i) {
      columns.push_back(arg[i]);
    }
    IACBridge::reset_stats_output();
    return;
  }

  if (std::strcmp(arg[0], "continue") == 0) {
    if (narg != 5 || std::strcmp(arg[1], "time") != 0 ||
        std::strcmp(arg[3], "variable") != 0) {
      error->all(FLERR, "Expected iac continue time <target> variable <name>");
    }
    const double target = std::atof(arg[2]);
    if (target <= 0.0) {
      error->all(FLERR, "iac continue time target must be positive");
    }
    try {
      double keep_running = 0.0;
      if (IACBridge::owns_model(sparta)) {
        keep_running = IACBridge::model(sparta).time() < target ? 1.0 : 0.0;
      }
      keep_running = broadcast_root_value(sparta, keep_running);
      set_internal_variable(input, error, arg[4], keep_running);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "limit") == 0) {
    if (narg != 3 || std::strcmp(arg[1], "time") != 0) {
      error->all(FLERR, "Expected iac limit time <target>");
    }
    const double target = std::atof(arg[2]);
    if (target <= 0.0) {
      error->all(FLERR, "iac limit time target must be positive");
    }
    try {
      if (IACBridge::owns_model(sparta)) {
        auto &model = IACBridge::model(sparta);
        const double remaining = target - model.time();
        if (remaining > 0.0 && model.timestep() > remaining) {
          model.set_timestep(remaining);
        }
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "set") == 0) {
    if (narg < 3) {
      error->all(FLERR, "Expected iac set <variable> <time|step|dt|diagnostic>");
    }
    try {
      if (std::strcmp(arg[2], "time") == 0) {
        if (narg != 3) {
          error->all(FLERR, "Expected iac set <variable> time");
        }
      } else if (std::strcmp(arg[2], "step") == 0) {
        if (narg != 3) {
          error->all(FLERR, "Expected iac set <variable> step");
        }
      } else if (std::strcmp(arg[2], "dt") == 0) {
        if (narg != 3) {
          error->all(FLERR, "Expected iac set <variable> dt");
        }
      } else if (std::strcmp(arg[2], "diagnostic") == 0) {
        if (narg != 4) {
          error->all(FLERR, "Expected iac set <variable> diagnostic <name>");
        }
      } else {
        error->all(FLERR, "iac set quantity must be time, step, dt, or diagnostic");
      }
      double value = 0.0;
      if (IACBridge::owns_model(sparta)) {
        auto &model = IACBridge::model(sparta);
        if (std::strcmp(arg[2], "time") == 0) {
          value = model.time();
        } else if (std::strcmp(arg[2], "step") == 0) {
          value = static_cast<double>(model.step_count());
        } else if (std::strcmp(arg[2], "dt") == 0) {
          value = model.timestep();
        } else if (std::strcmp(arg[2], "diagnostic") == 0) {
          value = model.diagnostic(arg[3]);
        }
      }
      value = broadcast_root_value(sparta, value);
      set_internal_variable(input, error, arg[1], value);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  if (std::strcmp(arg[0], "print") == 0) {
    if (narg != 2) {
      error->all(FLERR, "Illegal iac print command");
    }
    try {
      if (IACBridge::owns_model(sparta)) {
        const double value = IACBridge::model(sparta).diagnostic(arg[1]);
        if (screen) {
          std::fprintf(screen, "IAC diagnostic %s = %.17g\n", arg[1], value);
        }
        if (logfile) {
          std::fprintf(logfile, "IAC diagnostic %s = %.17g\n", arg[1], value);
        }
      }
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
    return;
  }

  error->all(FLERR, "Illegal iac command");
}
