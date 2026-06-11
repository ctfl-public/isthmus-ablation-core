#ifdef COMMAND_CLASS

CommandStyle(iac_continue,IacContinue)
CommandStyle(iac_limit,IacLimit)
CommandStyle(iac_run,IacRun)
CommandStyle(iac_set,IacSet)
CommandStyle(iac_spa_stats,IacSpaStats)
CommandStyle(iac_stats,IacStats)
CommandStyle(iac_stats_style,IacStatsStyle)
CommandStyle(iac_timestep,IacTimestep)
CommandStyle(iac_verify,IacVerify)

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

class IacContinue : protected Pointers {
public:
  explicit IacContinue(class SPARTA *);
  void command(int, char **);
};

class IacLimit : protected Pointers {
public:
  explicit IacLimit(class SPARTA *);
  void command(int, char **);
};

class IacRun : protected Pointers {
public:
  explicit IacRun(class SPARTA *);
  void command(int, char **);
};

class IacSet : protected Pointers {
public:
  explicit IacSet(class SPARTA *);
  void command(int, char **);
};

class IacStats : protected Pointers {
public:
  explicit IacStats(class SPARTA *);
  void command(int, char **);
};

class IacSpaStats : protected Pointers {
public:
  explicit IacSpaStats(class SPARTA *);
  void command(int, char **);
};

class IacStatsStyle : protected Pointers {
public:
  explicit IacStatsStyle(class SPARTA *);
  void command(int, char **);
};

class IacTimestep : protected Pointers {
public:
  explicit IacTimestep(class SPARTA *);
  void command(int, char **);
};

class IacVerify : protected Pointers {
public:
  explicit IacVerify(class SPARTA *);
  void command(int, char **);
};

} // namespace SPARTA_NS

#endif
#endif
