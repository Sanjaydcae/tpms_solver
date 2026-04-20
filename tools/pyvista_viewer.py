#!/usr/bin/env python3
"""Optional high-quality PyVista viewer for TPMS Studio exports."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _normalise_scalar_name(name: str) -> str:
    return name.replace(" ", "_").replace("-", "_").replace("/", "_").replace("\\", "_")


def _choose_scalar(mesh, requested: str) -> str | None:
    if requested in mesh.point_data:
        return requested

    normalised = _normalise_scalar_name(requested)
    if normalised in mesh.point_data:
        return normalised

    if mesh.point_data:
        return list(mesh.point_data.keys())[0]

    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Open a TPMS Studio mesh or result in PyVista.")
    parser.add_argument("mesh", help="Path to a VTK/VTU mesh file exported by TPMS Studio.")
    parser.add_argument("--scalar", default="", help="Point scalar to display as a contour result.")
    args = parser.parse_args()

    try:
        import pyvista as pv
    except Exception as exc:  # pragma: no cover - user environment guard
        print("PyVista is not installed.", file=sys.stderr)
        print("Install it with: python3 -m pip install --user pyvista", file=sys.stderr)
        print(f"Import error: {exc}", file=sys.stderr)
        return 2

    mesh_path = Path(args.mesh)
    if not mesh_path.exists():
        print(f"File not found: {mesh_path}", file=sys.stderr)
        return 2

    mesh = pv.read(str(mesh_path))
    surface = mesh.extract_surface()
    scalar = _choose_scalar(mesh, args.scalar) if args.scalar else _choose_scalar(mesh, "")

    plotter = pv.Plotter(title="TPMS Studio - PyVista Preview")
    plotter.set_background("#f7fbff", top="#dce9f6")

    if scalar:
        plotter.add_mesh(
            surface,
            scalars=scalar,
            cmap="turbo",
            show_edges=True,
            edge_color="#1f344a",
            line_width=0.25,
            smooth_shading=True,
            scalar_bar_args={
                "title": scalar.replace("_", " "),
                "vertical": True,
                "position_x": 0.03,
                "position_y": 0.18,
                "width": 0.08,
                "height": 0.62,
                "title_font_size": 12,
                "label_font_size": 10,
            },
        )
    else:
        plotter.add_mesh(
            surface,
            color="#6f9ec7",
            show_edges=True,
            edge_color="#23384f",
            line_width=0.25,
            smooth_shading=True,
        )

    plotter.add_axes()
    plotter.show_grid(color="#8fa6bd")
    plotter.add_text(
        "TPMS Studio - PyVista Preview",
        position="upper_left",
        font_size=10,
        color="#26384d",
    )
    plotter.camera_position = "iso"
    plotter.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
