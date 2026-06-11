#include "iacgrid.h"

#include "comm.h"
#include "error.h"
#include "fix.h"
#include "grid.h"
#include "iacbridge.h"
#include "modify.h"
#include "sparta.h"
#include "update.h"

#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

using namespace SPARTA_NS;

namespace {

std::string timestep_path(const char *pattern, bigint timestep) {
  std::string path(pattern);
  const std::size_t star = path.find('*');
  if (star == std::string::npos) {
    return path;
  }
  char step[64];
  std::snprintf(step, sizeof(step), "%06lld", static_cast<long long>(timestep));
  path.replace(star, 1, step);
  return path;
}

bigint output_index(SPARTA *sparta, const char *mode, Update *update, Error *error) {
  if (std::strcmp(mode, "dsmc-step") == 0) {
    return update->ntimestep;
  }
  if (std::strcmp(mode, "iac-step") == 0 || std::strcmp(mode, "iac-next") == 0) {
    bigint step = 0;
    if (IACBridge::owns_model(sparta)) {
      step = IACBridge::model(sparta).step_count();
      if (std::strcmp(mode, "iac-next") == 0) {
        ++step;
      }
    }
    MPI_Bcast(&step, 1, MPI_SPARTA_BIGINT, 0, sparta->world);
    return step;
  }
  error->all(FLERR, "grid_write_vtu index must be dsmc-step, iac-step, or iac-next");
  return update->ntimestep;
}

std::string xml_name(const char *name) {
  std::string result(name);
  for (char &ch : result) {
    if (ch == '[' || ch == ']' || ch == '/' || ch == ' ') {
      ch = '-';
    }
  }
  return result;
}

std::string data_array(const std::string &name, const std::vector<double> &values) {
  std::ostringstream out;
  out << "        <DataArray type=\"Float64\" Name=\"" << name
      << "\" format=\"ascii\">\n";
  out << std::setprecision(17);
  for (double value : values) {
    out << "          " << value << "\n";
  }
  out << "        </DataArray>\n";
  return out.str();
}

std::string data_array_int(const std::string &name, const std::vector<long long> &values) {
  std::ostringstream out;
  out << "        <DataArray type=\"Int64\" Name=\"" << name
      << "\" format=\"ascii\">\n";
  for (long long value : values) {
    out << "          " << value << "\n";
  }
  out << "        </DataArray>\n";
  return out.str();
}

Fix *require_grid_fix(Modify *modify, Error *error, const char *fix_id, int nfields) {
  const int ifix = modify->find_fix(fix_id);
  if (ifix < 0) {
    error->all(FLERR, "grid_write_vtu fix ID does not exist");
  }
  Fix *fix = modify->fix[ifix];
  if (!fix->per_grid_flag) {
    error->all(FLERR, "grid_write_vtu fix must provide per-grid data");
  }
  const int ncols = fix->size_per_grid_cols == 0 ? 1 : fix->size_per_grid_cols;
  if (nfields != ncols) {
    error->all(FLERR, "grid_write_vtu field count must match fix per-grid columns");
  }
  if (fix->size_per_grid_cols == 0 && !fix->vector_grid) {
    error->all(FLERR, "grid_write_vtu fix vector data is not available");
  }
  if (fix->size_per_grid_cols > 0 && !fix->array_grid) {
    error->all(FLERR, "grid_write_vtu fix array data is not available");
  }
  return fix;
}

void write_vtu(const std::string &path, const std::vector<double> &rows,
               int nfields, const std::vector<std::string> &field_names) {
  const int row_width = 7 + nfields;
  const int ncells = static_cast<int>(rows.size()) / row_width;
  std::vector<long long> ids;
  std::vector<double> field_values[64];
  if (nfields > 64) {
    throw std::runtime_error("grid_write_vtu supports at most 64 fields");
  }
  ids.reserve(static_cast<std::size_t>(ncells));
  for (int i = 0; i < nfields; ++i) {
    field_values[i].reserve(static_cast<std::size_t>(ncells));
  }

  std::ofstream out(path.c_str());
  if (!out) {
    throw std::runtime_error("Cannot open grid_write_vtu output " + path);
  }

  out << std::setprecision(17);
  out << "<?xml version=\"1.0\"?>\n";
  out << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  out << "  <UnstructuredGrid>\n";
  out << "    <Piece NumberOfPoints=\"" << 8 * ncells
      << "\" NumberOfCells=\"" << ncells << "\">\n";
  out << "      <Points>\n";
  out << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (int i = 0; i < ncells; ++i) {
    const double *row = &rows[static_cast<std::size_t>(i * row_width)];
    const double xlo = row[1], ylo = row[2], zlo = row[3];
    const double xhi = row[4], yhi = row[5], zhi = row[6];
    out << "          " << xlo << " " << ylo << " " << zlo << "\n";
    out << "          " << xhi << " " << ylo << " " << zlo << "\n";
    out << "          " << xhi << " " << yhi << " " << zlo << "\n";
    out << "          " << xlo << " " << yhi << " " << zlo << "\n";
    out << "          " << xlo << " " << ylo << " " << zhi << "\n";
    out << "          " << xhi << " " << ylo << " " << zhi << "\n";
    out << "          " << xhi << " " << yhi << " " << zhi << "\n";
    out << "          " << xlo << " " << yhi << " " << zhi << "\n";
    ids.push_back(static_cast<long long>(row[0]));
    for (int j = 0; j < nfields; ++j) {
      field_values[j].push_back(row[7 + j]);
    }
  }
  out << "        </DataArray>\n";
  out << "      </Points>\n";
  out << "      <Cells>\n";
  out << "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">\n";
  for (int i = 0; i < ncells; ++i) {
    const int base = 8 * i;
    out << "          " << base << " " << base + 1 << " " << base + 2 << " "
        << base + 3 << " " << base + 4 << " " << base + 5 << " "
        << base + 6 << " " << base + 7 << "\n";
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">\n";
  for (int i = 0; i < ncells; ++i) {
    out << "          " << 8 * (i + 1) << "\n";
  }
  out << "        </DataArray>\n";
  out << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n";
  for (int i = 0; i < ncells; ++i) {
    out << "          12\n";
  }
  out << "        </DataArray>\n";
  out << "      </Cells>\n";
  out << "      <CellData>\n";
  out << data_array_int("id", ids);
  for (int j = 0; j < nfields; ++j) {
    out << data_array(field_names[static_cast<std::size_t>(j)], field_values[j]);
  }
  out << "      </CellData>\n";
  out << "    </Piece>\n";
  out << "  </UnstructuredGrid>\n";
  out << "</VTKFile>\n";
}

} // namespace

namespace SPARTA_NS {

GridWriteVtu::GridWriteVtu(SPARTA *sparta) : Pointers(sparta) {}

GridDumpCommand::GridDumpCommand(SPARTA *sparta) : Pointers(sparta) {}

void iac_write_grid_vtu(SPARTA *sparta, const std::string &fix_id,
                        const std::string &path, const std::string &index_mode,
                        const std::vector<std::string> &field_names) {
  Grid *grid = sparta->grid;
  Modify *modify = sparta->modify;
  Error *error = sparta->error;
  Comm *comm = sparta->comm;
  Update *update = sparta->update;
  MPI_Comm world = sparta->world;

  Fix *fix = require_grid_fix(modify, error, fix_id.c_str(),
                              static_cast<int>(field_names.size()));

  const int nfields = static_cast<int>(field_names.size());
  const int row_width = 7 + nfields;
  std::vector<double> local;
  local.reserve(static_cast<std::size_t>(grid->nlocal * row_width));
  for (int i = 0; i < grid->nlocal; ++i) {
    const auto &cell = grid->cells[i];
    local.push_back(static_cast<double>(cell.id));
    local.push_back(cell.lo[0]);
    local.push_back(cell.lo[1]);
    local.push_back(cell.lo[2]);
    local.push_back(cell.hi[0]);
    local.push_back(cell.hi[1]);
    local.push_back(cell.hi[2]);
    if (fix->size_per_grid_cols == 0) {
      local.push_back(fix->vector_grid[i]);
    } else {
      for (int j = 0; j < nfields; ++j) {
        local.push_back(fix->array_grid[i][j]);
      }
    }
  }

  const int local_count = static_cast<int>(local.size());
  std::vector<int> counts(comm->nprocs, 0);
  MPI_Gather(&local_count, 1, MPI_INT, counts.data(), 1, MPI_INT, 0, world);
  std::vector<int> displs(comm->nprocs, 0);
  int total = 0;
  if (comm->me == 0) {
    for (int i = 0; i < comm->nprocs; ++i) {
      displs[i] = total;
      total += counts[i];
    }
  }
  std::vector<double> rows(static_cast<std::size_t>(total));
  MPI_Gatherv(local.data(), local_count, MPI_DOUBLE, rows.data(), counts.data(), displs.data(),
              MPI_DOUBLE, 0, world);

  if (comm->me == 0) {
    try {
      write_vtu(timestep_path(path.c_str(), output_index(sparta, index_mode.c_str(), update, error)),
                rows, nfields, field_names);
    } catch (const std::exception &ex) {
      error->all(FLERR, ex.what());
    }
  }
}

void GridDumpCommand::command(int narg, char **arg) {
  if (narg < 7) {
    error->all(FLERR, "Expected grid_dump <id> <fix-id> vtu <N> <path> [index <mode>] fields <name...>");
  }
  if (std::strcmp(arg[2], "vtu") != 0) {
    error->all(FLERR, "grid_dump currently supports only vtu output");
  }
  const int every = std::atoi(arg[3]);
  if (every <= 0) {
    error->all(FLERR, "grid_dump interval must be positive");
  }
  const char *index_mode = "dsmc-step";
  int fields = 5;
  if (std::strcmp(arg[fields], "index") == 0) {
    if (narg < 9) {
      error->all(FLERR, "Expected grid_dump index <mode> fields <name...>");
    }
    index_mode = arg[fields + 1];
    fields += 2;
  }
  if (fields >= narg || std::strcmp(arg[fields], "fields") != 0) {
    error->all(FLERR, "Expected grid_dump <id> <fix-id> vtu <N> <path> [index <mode>] fields <name...>");
  }
  const int nfields = narg - fields - 1;
  if (nfields <= 0) {
    error->all(FLERR, "grid_dump requires at least one field name");
  }
  std::vector<std::string> field_names;
  field_names.reserve(static_cast<std::size_t>(nfields));
  for (int i = fields + 1; i < narg; ++i) {
    field_names.push_back(xml_name(arg[i]));
  }
  require_grid_fix(modify, error, arg[1], nfields);
  IACBridge::record_grid_vtu_dump(arg[1], arg[4], index_mode, field_names, every);
}

void GridWriteVtu::command(int narg, char **arg) {
  if (narg < 4) {
    error->all(FLERR, "Expected grid_write_vtu <fix-id> <path> [index <mode>] fields <name...>");
  }
  const char *index_mode = "dsmc-step";
  int fields = 2;
  if (std::strcmp(arg[fields], "index") == 0) {
    if (narg < 6) {
      error->all(FLERR, "Expected grid_write_vtu index <mode> fields <name...>");
    }
    index_mode = arg[fields + 1];
    fields += 2;
  }
  if (fields >= narg || std::strcmp(arg[fields], "fields") != 0) {
    error->all(FLERR, "Expected grid_write_vtu <fix-id> <path> [index <mode>] fields <name...>");
  }
  const int nfields = narg - fields - 1;
  if (nfields <= 0) {
    error->all(FLERR, "grid_write_vtu requires at least one field name");
  }
  std::vector<std::string> field_names;
  field_names.reserve(static_cast<std::size_t>(nfields));
  for (int i = fields + 1; i < narg; ++i) {
    field_names.push_back(xml_name(arg[i]));
  }
  IACBridge::record_grid_vtu_dump(arg[0], arg[1], index_mode, field_names);
  iac_write_grid_vtu(sparta, arg[0], arg[1], index_mode, field_names);
}

} // namespace SPARTA_NS
