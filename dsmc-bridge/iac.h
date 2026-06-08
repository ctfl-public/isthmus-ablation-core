#ifdef COMMAND_CLASS

CommandStyle(iac,Iac)

#else

#ifndef SPARTA_IAC_H
#define SPARTA_IAC_H

#include "pointers.h"

namespace SPARTA_NS {

class Iac : protected Pointers {
public:
  explicit Iac(class SPARTA *);
  void command(int, char **);
};

} // namespace SPARTA_NS

#endif
#endif
