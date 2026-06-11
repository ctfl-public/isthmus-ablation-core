#ifdef COMMAND_CLASS

CommandStyle(voxel_ablate,VoxelAblate)
CommandStyle(voxel_create,VoxelCreate)
CommandStyle(voxel_dump,VoxelDumpCommand)
CommandStyle(voxel_ghost,VoxelGhost)
CommandStyle(voxel_material,VoxelMaterial)
CommandStyle(voxel_write_history,VoxelWriteHistory)
CommandStyle(voxel_write_vtu,VoxelWriteVtu)

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

class VoxelAblate : protected Pointers {
 public:
  VoxelAblate(class SPARTA *);
  void command(int, char **);
};

class VoxelCreate : protected Pointers {
 public:
  VoxelCreate(class SPARTA *);
  void command(int, char **);
};

class VoxelDumpCommand : protected Pointers {
 public:
  VoxelDumpCommand(class SPARTA *);
  void command(int, char **);
};

class VoxelGhost : protected Pointers {
 public:
  VoxelGhost(class SPARTA *);
  void command(int, char **);
};

class VoxelMaterial : protected Pointers {
 public:
  VoxelMaterial(class SPARTA *);
  void command(int, char **);
};

class VoxelWriteHistory : protected Pointers {
 public:
  VoxelWriteHistory(class SPARTA *);
  void command(int, char **);
};

class VoxelWriteVtu : protected Pointers {
 public:
  VoxelWriteVtu(class SPARTA *);
  void command(int, char **);
};

}

#endif
#endif
