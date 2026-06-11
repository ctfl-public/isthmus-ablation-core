#ifdef COMMAND_CLASS

CommandStyle(grid_dump,GridDumpCommand)
CommandStyle(grid_write_vtu,GridWriteVtu)

#else

#ifndef SPARTA_IACGRID_H
#define SPARTA_IACGRID_H

#include "pointers.h"

#include <string>
#include <vector>

namespace SPARTA_NS {

void iac_write_grid_vtu(class SPARTA *, const std::string &fix_id,
                        const std::string &path, const std::string &index_mode,
                        const std::vector<std::string> &field_names);

class GridDumpCommand : protected Pointers {
 public:
  explicit GridDumpCommand(class SPARTA *);
  void command(int, char **);
};

class GridWriteVtu : protected Pointers {
 public:
  explicit GridWriteVtu(class SPARTA *);
  void command(int, char **);
};

} // namespace SPARTA_NS

#endif
#endif
