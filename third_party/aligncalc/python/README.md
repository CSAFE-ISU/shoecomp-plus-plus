# aligncalc (Python)

Rigid (RT) and similarity (RTS) matching of point sets and 3-D line
segments without an initial guess: a correspondence graph over all
candidate pairs, maximum cliques, then distinct alignments, best
first.

```python
import numpy as np
import aligncalc

res = aligncalc.align_points(left, right, mode="rts", results=2)
for a in res:
    print(len(a.pairs), a.scale, a.angle, a.translation)
    moved = a.apply(left)          # left points in the right frame

lines = aligncalc.align_lines_3d(left_segments, right_segments,
                                 angle_tol_deg=0.5, sep_tol=4e-6)
```

Options can also come from scpp's INI sections, e.g.
`align_points(l, r, align2d=cfg["align2d"], clique=cfg["clique"])`
with a `configparser` file. See `help(aligncalc.align_points)`.

Install from a checkout of the repository (the C++ library is in the
parent directory):

```
pip install ./third_party/aligncalc/python
pip install pytest && pytest third_party/aligncalc/python/tests
```

Needs a C++17 compiler and CMake >= 3.23; Eigen is the bundled
submodule (`third_party/aligncalc/eigen`).
