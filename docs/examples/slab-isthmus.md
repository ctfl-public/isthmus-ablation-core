# ISTHMUS Slab Ablation

These examples ablate an 8 by 4 by 4 slab from the `xlo` ISTHMUS surface.

The finite-patch case intentionally exposes lateral edges:

```text
examples/slab-isthmus-ablation/in.slab-isthmus-finite
```

It runs:

```text
isthmus_surface skin voxels solid buffer 3 iso 0.45 map yes
surf_flux skin source q1 select normal nx -1.0 ny 0.0 nz 0.0 min-cos 0.5
voxel_ablate solid surface skin policy local delete yes
```

The ghost case adds y/z infinite-wall ghost voxels before surface generation:

```text
voxel_ghost solid axis y boundary infinite layers 1
voxel_ghost solid axis z boundary infinite layers 1
isthmus_surface skin voxels solid buffer 3 map yes
```

Ghost voxels are surface-construction images only. They do not carry separate
mass, and triangle ownership maps back to the corresponding real voxels. This
removes finite-patch edge effects while keeping the ablation update on the real
voxel domain.

Run either case:

```bash
build/ia-core -in examples/slab-isthmus-ablation/in.slab-isthmus-finite
build/ia-core -in examples/slab-isthmus-ablation/in.slab-isthmus-ghost
```

The regression wrappers are:

```text
tests/inputs/slab-isthmus-ablation/in.slab-isthmus-finite-surface.verify
tests/inputs/slab-isthmus-ablation/in.slab-isthmus-ghost-wall.verify
```

The finite-patch wrapper uses broad tolerances because the exposed edges are
expected to perturb the one-dimensional exact recession. The ghost wrapper uses
tight tolerances because the infinite-wall construction should recover the
one-dimensional slab solution.
