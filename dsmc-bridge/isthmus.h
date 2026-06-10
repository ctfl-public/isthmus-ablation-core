#ifdef COMMAND_CLASS

CommandStyle(isthmus,Isthmus)
CommandStyle(isthmus_surf,IsthmusSurf)

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

class IsthmusSurf : protected Pointers {
 public:
  IsthmusSurf(class SPARTA *);
  void command(int, char **);
};

}

#endif
#endif
