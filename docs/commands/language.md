# Input Language Map

The input language is split by ownership. IAC commands use explicit prefixes so
they can sit beside native DSMC/SPARTA commands without ambiguity.

## Command Families

| Family | Owns | Typical commands |
| --- | --- | --- |
| `voxel_*` | Solid voxel state, material assignment, voxel dumps, mass removal | `voxel_material`, `voxel_create`, `voxel_ghost`, `voxel_ablate`, `voxel_dump` |
| `isthmus_*` | ISTHMUS reconstruction from voxels | `isthmus_surface` |
| `surf_*` | IAC surface flux, surface installation, surface output, DSMC surface measurements | `surf_flux`, `surf_install`, `surf_measure_flux`, `surf_dump`, `surf_write_vtp` |
| `iac_*` | IAC solid time, stats, verification, bridge variables | `iac_timestep`, `iac_limit`, `iac_run`, `iac_continue`, `iac_verify`, `iac_stats` |
| Native DSMC/SPARTA | Gas domain, particles, DSMC timestepping, collision/reaction models | `create_grid`, `species`, `mixture`, `surf_modify`, `run`, `timestep`, `stats` |

## Time Advancement

In standalone mode, `iac_run` is the only timestep advancement command. In
DSMC-hosted mode, native `run` advances particles and collisions, while
`iac_run` advances only the solid voxel ledger.

Long commands can be split across physical lines with a trailing `&`:

```text
surf_flux           skin dsmc/reaction fix rco column 1 sample-steps 100 &
                      reaction carbon-co.surf mass-courant 1.0 &
                      select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5
```

The standalone parser joins these lines before tokenizing, matching DSMC/SPARTA
input syntax.

The usual solid update order is:

```text
iac_limit           time ${ablation_time}
isthmus_surface     skin voxels solid buffer 1 weighting no map yes
surf_flux           skin source q1 select all
voxel_ablate        solid surface skin policy carryover/normal delete yes
iac_run             1
```

For coupled DSMC, insert native DSMC sampling before the surface flux command:

```text
fix                 sflux ave/surf all 1 100 100 c_sflux[*] ave one
run                 100 post no
surf_flux           skin dsmc/surf fix sflux mass-courant 0.1666666667
```

## Portable Time Loops

Use this form when an input should run both standalone and through the DSMC
bridge:

```text
variable            ablation_time equal 4.0e-3
variable            keep internal 1
label               ablate-loop

iac_limit           time ${ablation_time}
voxel_ablate        solid source q1 policy local face xlo delete yes
iac_run             1

iac_continue        time ${ablation_time} variable keep
if                  "${keep} > 0" then "jump SELF ablate-loop"
```

The standalone parser recognizes this compact DSMC-style loop pattern. DSMC
executes it with its normal variable, `if`, and `jump` machinery.

## Verification Inputs

Examples should be runnable and visual by default. Tests should usually include
an example, turn off expensive dumps, and add criteria:

```text
include             examples/slab-direct-ablation/in.slab-direct-ablation

voxel_dump          off
surf_dump           off

iac_verify          mf exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
```

This keeps examples and tests from drifting apart while leaving examples useful
for visual inspection.
