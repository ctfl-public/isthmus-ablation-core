#ifdef COMMAND_CLASS

CommandStyle(source,Source)

#else

#ifndef SPARTA_IAC_SOURCE_H
#define SPARTA_IAC_SOURCE_H

#include "pointers.h"

namespace SPARTA_NS {

class Source : protected Pointers {
public:
  explicit Source(class SPARTA *);
  void command(int, char **);
};

} // namespace SPARTA_NS

#endif
#endif
