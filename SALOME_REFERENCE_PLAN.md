# SALOME Reference Plan for TPMS Studio

TPMS Studio can use SALOME as a workflow and architecture reference, but must not copy
SALOME source code directly into this proprietary repository. The SALOME package inspected
at `/home/sanjay/Downloads/SALOME-9.15.0-native-UB24.04-SRC` includes LGPL-licensed
components and many third-party modules with their own licenses.

## What SALOME Teaches Us

- Keep large CAE systems modular:
  - `GUI` for application shell and viewers.
  - `GEOM` / `SHAPER` for CAD and geometry.
  - `SMESH` for mesh data structures and meshing workflows.
  - `GMSHPLUGIN` and `NETGENPLUGIN` for external meshing backends.
  - `PARAVIS` / ParaView for advanced result visualization.
  - `JOBMANAGER` for external solver/job execution.
- Treat meshing and solving as backends, not UI tricks.
- Provide a launcher/environment layer for dependency discovery.
- Provide clear system checks so users know whether OpenGL, solver, and mesher backends are available.
- Keep visual post-processing separate from simulation data storage.

## Safe Adoption Rules

- Do not copy SALOME C++/Python source into TPMS Studio.
- Do not vendor SALOME binaries into this proprietary repository.
- Use SALOME only as a design reference for module boundaries and workflow behavior.
- Integrate external tools through command-line or documented APIs:
  - `gmsh` for surface-conforming tetra meshing.
  - `netgen` as an optional alternative mesher.
  - `ccx` / CalculiX for solver execution.
  - PyVista/VTK for visualization.

## TPMS Studio Target Architecture

```text
TPMS Studio
├── GUI Shell
│   ├── Model Tree
│   ├── Properties / Details panel
│   ├── VTK viewport
│   └── Log / job monitor
├── Geometry Module
│   ├── TPMS scalar field
│   ├── surface extraction
│   └── STL/OBJ export for meshing backend
├── Mesh Module
│   ├── Tri3 surface mesh
│   ├── Tet4 volume mesh
│   ├── Gmsh backend
│   ├── NetGen backend later
│   └── quality checks
├── Model Module
│   ├── materials
│   ├── node sets / element sets
│   ├── boundary conditions
│   └── load cases
├── Job Module
│   ├── CalculiX input writer
│   ├── solver launcher
│   ├── job progress
│   └── log/error parser
└── Results Module
    ├── displacement components
    ├── stress / strain
    ├── reaction force
    ├── scalar range controls
    └── exports
```

## Immediate SALOME-Inspired Changes

1. Add a backend status panel:
   - VTK: available / missing.
   - Gmsh: available / missing.
   - NetGen: available / missing.
   - CalculiX: available / missing.
   - PyVista: available / optional.
2. Add a real `Job` concept:
   - job directory,
   - input file path,
   - solver executable,
   - log file,
   - result file,
   - run state.
3. Replace voxel-tet volume meshing with Gmsh first:
   - export TPMS surface as watertight STL,
   - call `gmsh -3`,
   - import nodes and Tet4 cells,
   - run quality checks.
4. Keep NetGen as second backend, after Gmsh is stable.
5. Add a launcher/dependency check command in the Help or Tools menu.

## Commercial Product Position

Recommended licensing model:

- TPMS Studio source: proprietary H2one Cleantech code.
- Gmsh, NetGen, CalculiX, SALOME: external references/tools only.
- Do not ship GPL/LGPL source inside the proprietary repository unless legal review approves the distribution model.

