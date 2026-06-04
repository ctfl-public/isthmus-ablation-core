#ifdef COMMAND_CLASS

CommandStyle(surface,Surface)

#else

#ifndef SPARTA_SURFACE_H
#define SPARTA_SURFACE_H

#include "pointers.h"

namespace SPARTA_NS {

class Surface : protected Pointers {
 public:
  Surface(class SPARTA *);
  void command(int, char **);
};

}

#endif
#endif
