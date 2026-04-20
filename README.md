# TPMS Studio

**TPMS Studio** is a desktop CAE prototype for creating, meshing, solving, and post-processing TPMS-based lattice structures.

**Owner / License Holder:** H2one Cleantech Private Limited  
**License Type:** Proprietary Commercial / Internal Development License

> Copyright (c) 2026 H2one Cleantech Private Limited. All rights reserved.

## Current Capabilities

- TPMS geometry generation: Gyroid, Diamond, Schwarz-P, Lidinoid, Neovius
- Geometry preview with 3D viewport controls
- Surface and volume meshing workflow
- Material assignment for linear elastic isotropic materials
- Boundary conditions: fixed support, force load, displacement load
- Linear static solver workflow with live progress
- FEM post-processing:
  - Total displacement
  - Von Mises stress
  - Equivalent strain
- Active result export to VTK
- Mesh and geometry export
- Markdown report export
- Model health check

## Important Engineering Note

This project is currently a working MVP/prototype CAE tool. It is not yet a certified commercial solver. For real engineering use, results must be validated against analytical benchmarks, reference solvers, or experimental data.

Large compression cases, buckling, contact, plasticity, and nonlinear geometry are outside the current linear-static solver scope.

## Requirements

Ubuntu/Debian packages:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  git \
  libglfw3-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  libxext-dev \
  libvtk9-dev
```

Optional for GitHub upload:

```bash
sudo apt-get install -y gh
gh auth login
```

Optional for high-quality PyVista preview:

```bash
sudo apt-get install -y python3-pip python3-venv
python3 -m pip install --user pyvista
```

After generating a volume mesh or solver result, use `Results > Open in PyVista` to open an interactive external preview with rotation, axes, mesh edges, and result colour maps.

## Build

```bash
cd /home/sanjay/Desktop/tpms_solver
bash build.sh
```

Or manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run

```bash
./build/tpms_solver
```

## Example Compression Workflow

For a small Diamond TPMS compression test:

1. Geometry:
   - TPMS Type: `Diamond`
   - Domain Size: `15 x 15 x 15 mm`
   - Unit Cell: `5 x 5 x 5 mm`
   - Wall Thickness: `0.8 to 1.5 mm`
   - Resolution: `80`

2. Meshing:
   - Generate Surface Mesh
   - Generate Volume Mesh

3. Boundary Conditions:
   - Assign Material
   - Fixed Support: Bottom face, Z-min
   - Displacement Load: Top face, Z-max
   - Displacement: `0, 0, -1.0 mm`

4. Solve:
   - Validate Model
   - Solve

5. Results:
   - Total Displacement
   - Von Mises Stress
   - Equivalent Strain

For a 15 mm height compressed by 1 mm, the engineering strain is about 6.67 percent. Treat the current result as a linear-static approximation.

## Repository Hygiene

Do not commit:

- `build/`
- generated binaries
- generated result files
- local runtime settings such as `tpms_studio.ini`

The `.gitignore` file is configured for this.
