#ifdef COMMAND_CLASS

CommandStyle(isthmus_surface,IsthmusSurface)

#else

#ifndef SPARTA_ISTHMUS_H
#define SPARTA_ISTHMUS_H

#include "pointers.h"

namespace SPARTA_NS {

class Isthmus : protected Pointers {
 public:
  Isthmus(class SPARTA *);
  void command(int, char **);
};

class IsthmusSurface : protected Pointers {
 public:
  IsthmusSurface(class SPARTA *);
  void command(int, char **);
};

}

#endif
#endif
