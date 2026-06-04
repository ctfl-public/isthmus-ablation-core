#ifdef COMMAND_CLASS

CommandStyle(voxel,Voxel)

#else

#ifndef SPARTA_VOXEL_H
#define SPARTA_VOXEL_H

#include "pointers.h"

namespace SPARTA_NS {

class Voxel : protected Pointers {
 public:
  Voxel(class SPARTA *);
  void command(int, char **);
};

}

#endif
#endif
