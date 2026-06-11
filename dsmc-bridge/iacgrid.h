#ifdef COMMAND_CLASS

CommandStyle(grid_write_vtu,GridWriteVtu)

#else

#ifndef SPARTA_IACGRID_H
#define SPARTA_IACGRID_H

#include "pointers.h"

namespace SPARTA_NS {

class GridWriteVtu : protected Pointers {
 public:
  explicit GridWriteVtu(class SPARTA *);
  void command(int, char **);
};

} // namespace SPARTA_NS

#endif
#endif
