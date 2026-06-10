#ifdef COMMAND_CLASS

CommandStyle(surface,Surface)
CommandStyle(surf_dump,SurfaceDumpCommand)
CommandStyle(surf_flux,SurfaceFlux)
CommandStyle(surf_install,SurfaceInstall)
CommandStyle(surf_measure_flux,SurfaceMeasureFlux)
CommandStyle(surf_write_vtp,SurfaceWriteVtp)

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

class SurfaceDumpCommand : protected Pointers {
 public:
  SurfaceDumpCommand(class SPARTA *);
  void command(int, char **);
};

class SurfaceFlux : protected Pointers {
 public:
  SurfaceFlux(class SPARTA *);
  void command(int, char **);
};

class SurfaceInstall : protected Pointers {
 public:
  SurfaceInstall(class SPARTA *);
  void command(int, char **);
};

class SurfaceMeasureFlux : protected Pointers {
 public:
  SurfaceMeasureFlux(class SPARTA *);
  void command(int, char **);
};

class SurfaceWriteVtp : protected Pointers {
 public:
  SurfaceWriteVtp(class SPARTA *);
  void command(int, char **);
};

}

#endif
#endif
