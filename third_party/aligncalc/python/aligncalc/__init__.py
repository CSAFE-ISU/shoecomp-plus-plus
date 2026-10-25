"""Rigid (RT) and similarity (RTS) matching of point sets and 3-D line
segments without an initial guess (correspondence graph + maximum
cliques).

    align_points(left, right, ...)    -> [PointAlignment, ...]
    match_points(left, right, ...)    -> [PointMatch, ...] (no fit)
    align_lines_3d(left, right, ...)  -> [LineAlignment, ...]

Each call returns its distinct alignments, best first, or [] when
nothing matches. Options are keyword arguments; scpp's INI sections can
be passed as mappings (values may be strings, as configparser gives
them), e.g.

    cfg = configparser.ConfigParser(); cfg.read("params.ini")
    align_points(l, r, align2d=cfg["align2d"], clique=cfg["clique"])
    align_points(l, r, clique={"min_matched": 5}, **align2d_dict)

A key may be given once, either as a keyword or in a section mapping.
"""

from dataclasses import dataclass
import math

import numpy as np

from . import _core

__version__ = getattr(_core, "__version__", "unknown")

__all__ = [
    "align_points",
    "match_points",
    "align_lines_3d",
    "PointAlignment",
    "PointMatch",
    "LineAlignment",
    "Cancelled",
    "__version__",
]


class Cancelled(Exception):
    """The progress callback asked to stop."""


@dataclass(frozen=True)
class PointAlignment:
    """T(x) = scale * rotation @ x + translation maps left onto right."""

    pairs: np.ndarray  # (k, 2) int32: left row, right row
    rotation: np.ndarray  # (d, d)
    scale: float  # 1 for rigid matching
    translation: np.ndarray  # (d,)
    angle: float  # 2-D: rotation angle (radians); else 0
    explained: int  # pairs the alignment accounts for

    def apply(self, points):
        """Map left-frame points (n, d) into the right frame."""
        p = np.asarray(points, dtype=np.float64)
        return self.scale * p @ self.rotation.T + self.translation


@dataclass(frozen=True)
class PointMatch:
    """Matched points, no transform (match_points)."""

    pairs: np.ndarray  # (k, 2) int32: left row, right row
    left_points: np.ndarray  # (k, d) matched left positions
    right_points: np.ndarray  # (k, d) matching right positions


@dataclass(frozen=True)
class LineAlignment:
    """x -> rotation @ x + translation maps left onto right."""

    pairs: np.ndarray  # (k, 2) int32: left row, right row
    rotation: np.ndarray  # (3, 3)
    translation: np.ndarray  # (3,)
    axis: np.ndarray  # (3,) bundle direction, right frame
    rms_perp: float  # midpoint-to-line distance, right frame
    axial_weak: bool  # translation along axis only from midpoints
    explained: int  # pairs the alignment accounts for

    @property
    def matrix(self):
        """4 x 4 homogeneous transform."""
        m = np.eye(4)
        m[:3, :3] = self.rotation
        m[:3, 3] = self.translation
        return m

    def apply(self, points):
        """Map left-frame points (n, 3) into the right frame."""
        p = np.asarray(points, dtype=np.float64)
        return p @ self.rotation.T + self.translation


# ---- option handling -------------------------------------------------

_TRUE = {"true", "1", "yes", "on"}
_FALSE = {"false", "0", "no", "off"}


def _as_float(name, v):
    if isinstance(v, bool):
        raise TypeError(f"{name} must be a number, not bool")
    try:
        x = float(v.strip() if isinstance(v, str) else v)
    except (TypeError, ValueError):
        raise TypeError(f"{name} must be a number, got {v!r}") from None
    if not math.isfinite(x):
        raise ValueError(f"{name} must be finite")
    return x


def _as_int(name, v):
    if isinstance(v, bool):
        raise TypeError(f"{name} must be an integer, not bool")
    if isinstance(v, str):
        try:
            return int(v.strip())
        except ValueError:
            raise TypeError(f"{name} must be an integer, got {v!r}") from None
    if isinstance(v, (int, np.integer)):
        return int(v)
    if isinstance(v, (float, np.floating)) and float(v).is_integer():
        return int(v)
    raise TypeError(f"{name} must be an integer, got {v!r}")


def _as_bool(name, v):
    if isinstance(v, (bool, np.bool_)):
        return bool(v)
    if isinstance(v, str) and v.strip().lower() in _TRUE | _FALSE:
        return v.strip().lower() in _TRUE
    if isinstance(v, (int, np.integer)) and v in (0, 1):
        return bool(v)
    raise TypeError(f"{name} must be a bool, got {v!r}")


def _as_str(name, v):
    if not isinstance(v, str):
        raise TypeError(f"{name} must be a string, got {v!r}")
    return v.strip().lower()


_CLIQUE = {
    "min_matched": (_as_int, 3),
    "max_matched": (_as_int, 0),
    "results": (_as_int, 2),
    "distinct": (_as_str, "explained"),
    "distinct_min_frac": (_as_float, 0.7),
}

# match_points: no transform, so only disjoint peeling
_CLIQUE_MATCH = dict(_CLIQUE, distinct=(_as_str, "disjoint"))

_POINTS = {
    "mode": (_as_str, None),  # "rt" | "rts"; default from same_scale
    "same_scale": (_as_bool, None),  # [align2d]: True = "rt"
    "epsilon": (_as_float, 0.005),
    "scale_range": (_as_float, 5.0),
    "anchor_tol": (_as_float, 0.05),
    "min_support": (_as_int, 1),
    "orientation_tol_deg": (_as_float, None),
    "minutia_angle_tol_deg": (_as_float, None),  # [align2d] alias
}

_LINES = {
    "angle_tol_deg": (_as_float, 5.0),
    "sep_tol": (_as_float, None),  # position units
    "sep_tol_um": (_as_float, None),  # [align3d]: positions in metres
    "anchor_tol": (_as_float, None),  # position units
    "anchor_tol_um": (_as_float, None),  # [align3d]: positions in metres
    "extent_scale": (_as_float, 0.0),
    # scpp-only rules (striation length ratio, rotation limit): not
    # part of aligncalc, accepted only as 0 = off
    "max_length_ratio": (_as_float, 0.0),
    "max_rotation_deg": (_as_float, 0.0),
}


def _merge(groups, kwargs):
    """groups: [(section name, section mapping or None, spec)].
    Returns {key: converted value or None (unset)} over all specs."""
    given = {}
    for section, mapping, spec in groups:
        if mapping is None:
            continue
        for key, value in dict(mapping).items():
            if key not in spec:
                raise TypeError(f"unknown [{section}] key {key!r}")
            if key in given:
                raise TypeError(f"{key!r} given twice")
            given[key] = value
    allowed = {}
    for _, _, spec in groups:
        allowed.update(spec)
    for key, value in kwargs.items():
        if key not in allowed:
            raise TypeError(f"unexpected keyword argument {key!r}")
        if value is None:
            continue
        if key in given:
            raise TypeError(f"{key!r} given twice")
        given[key] = value
    out = {}
    for key, (conv, default) in allowed.items():
        out[key] = conv(key, given[key]) if key in given else default
    return out


def _one_of(o, a, b, default):
    """Value of alias pair (a, b) in o; at most one may be set."""
    if o[a] is not None and o[b] is not None:
        raise TypeError(f"give {a!r} or {b!r}, not both")
    return o[a] if o[a] is not None else o[b] if o[b] is not None else default


def _metres(o, key, default_um):
    """o[key] (position units) or o[key + "_um"] * 1e-6, not both."""
    um = key + "_um"
    if o[key] is not None and o[um] is not None:
        raise TypeError(f"give {key!r} or {um!r}, not both")
    if o[key] is not None:
        return o[key]
    return (o[um] if o[um] is not None else default_um) * 1e-6


def _clique_args(o, workers):
    if o["min_matched"] < 2:
        raise ValueError("min_matched must be >= 2")
    if o["max_matched"] < 0:
        raise ValueError("max_matched must be >= 0 (0 = no cap)")
    if o["results"] < 1:
        raise ValueError("results must be >= 1")
    if o["distinct"] not in ("explained", "disjoint"):
        raise ValueError("distinct must be 'explained' or 'disjoint'")
    if not 0.0 <= o["distinct_min_frac"] <= 1.0:
        raise ValueError("distinct_min_frac must be in [0, 1]")
    workers = _as_int("workers", workers)
    if workers < 0:
        raise ValueError("workers must be >= 0 (0 = all cores)")
    return dict(
        min_matched=o["min_matched"],
        max_matched=o["max_matched"],
        results=o["results"],
        disjoint=o["distinct"] == "disjoint",
        distinct_min_frac=o["distinct_min_frac"],
        workers=workers,
    )


def _rows(name, a, ncols):
    a = np.asarray(a, dtype=np.float64)
    if a.ndim == 1 and ncols == (1, None):
        a = a.reshape(-1, 1)
    lo, hi = ncols
    if a.ndim != 2 or not lo <= a.shape[1] <= (hi or a.shape[1]):
        want = (f"{lo}" if lo == hi else
                f"{lo}-{hi}" if hi else "d >= 1")
        raise ValueError(f"{name} must be an (n, {want}) array")
    if not np.all(np.isfinite(a)):
        raise ValueError(f"{name} has non-finite values")
    return a


def _pair(name, value, n_left, n_right):
    """(left, right) per-row arrays, or None."""
    if value is None:
        return None
    try:
        a, b = value
    except (TypeError, ValueError):
        raise TypeError(f"{name} must be a (left, right) pair") from None
    if a is None and b is None:
        return None
    if a is None or b is None:
        raise ValueError(f"{name}: give both sides or neither")
    a, b = np.asarray(a).ravel(), np.asarray(b).ravel()
    if len(a) != n_left or len(b) != n_right:
        raise ValueError(f"{name} must have one entry per row")
    return a, b


def _label_codes(labels):
    """Any hashable labels -> float codes, equal iff labels are equal."""
    a, b = labels
    _, inv = np.unique(np.concatenate([a, b]), return_inverse=True)
    inv = inv.astype(np.float64).ravel()
    return inv[: len(a)], inv[len(a) :]


def _finish(status, message, alignments, make):
    if status == _core.Status.Ok:
        return [make(**a) for a in alignments]
    if status == _core.Status.NoMatch:
        return []
    if status == _core.Status.Cancelled:
        raise Cancelled(message)
    raise ValueError(message)


def _point_args(left, right, labels, orientations, workers, align2d,
                clique, clique_spec, kwargs):
    """Checked _core arguments for align_points / match_points, and
    the point positions (n, d) of both sides."""
    o = _merge(
        [("align2d", align2d, _POINTS), ("clique", clique, clique_spec)],
        kwargs,
    )
    if o["mode"] is not None and o["mode"] not in ("rt", "rts"):
        raise ValueError("mode must be 'rt' or 'rts'")
    if o["mode"] is not None and o["same_scale"] is not None:
        if (o["mode"] == "rt") != o["same_scale"]:
            raise ValueError("mode and same_scale disagree")
    similarity = (
        o["mode"] == "rts"
        if o["mode"] is not None
        else o["same_scale"] is False
    )
    tol_deg = _one_of(o, "orientation_tol_deg", "minutia_angle_tol_deg", 0.0)
    if not o["epsilon"] > 0:
        raise ValueError("epsilon must be > 0")
    if not o["scale_range"] >= 1:
        raise ValueError("scale_range must be >= 1")
    if not o["anchor_tol"] >= 0:
        raise ValueError("anchor_tol must be >= 0")
    if o["min_support"] < 1:
        raise ValueError("min_support must be >= 1")
    if not 0 <= tol_deg <= 180:
        raise ValueError("orientation_tol_deg must be in [0, 180]")
    c = _clique_args(o, workers)

    L = _rows("left", left, (1, None))
    R = _rows("right", right, (1, None))
    dim = L.shape[1]
    if R.shape[1] != dim:
        raise ValueError("left and right must have the same dimension")
    if similarity and dim < 2:
        raise ValueError(
            "similarity ('rts') matching needs 2 or more dimensions")
    lab = _pair("labels", labels, len(L), len(R))
    ori = _pair("orientations", orientations, len(L), len(R))
    if ori is not None and dim != 2:
        raise ValueError("orientations need 2-D points")
    cols_l, cols_r = [L], [R]
    label_col = angle_col = -1
    if lab is not None:
        a, b = _label_codes(lab)
        label_col = dim
        cols_l.append(a[:, None])
        cols_r.append(b[:, None])
    if ori is not None:
        angle_col = dim + (lab is not None)
        a = _rows("orientations[0]", ori[0][:, None], (1, 1))
        b = _rows("orientations[1]", ori[1][:, None], (1, 1))
        cols_l.append(a)
        cols_r.append(b)
    args = dict(
        left=np.ascontiguousarray(np.hstack(cols_l)),
        right=np.ascontiguousarray(np.hstack(cols_r)),
        dim=dim,
        label_col=label_col,
        angle_col=angle_col,
        similarity=similarity,
        epsilon=o["epsilon"],
        scale_range=o["scale_range"],
        anchor_tol=o["anchor_tol"],
        min_support=o["min_support"],
        orientation_tol_deg=tol_deg,
        **c,
    )
    return args, L, R


# ---- entry points ----------------------------------------------------


def align_points(
    left,
    right,
    *,
    labels=None,
    orientations=None,
    workers=0,
    progress=None,
    align2d=None,
    clique=None,
    mode=None,
    same_scale=None,
    epsilon=None,
    scale_range=None,
    anchor_tol=None,
    min_support=None,
    orientation_tol_deg=None,
    minutia_angle_tol_deg=None,
    min_matched=None,
    max_matched=None,
    results=None,
    distinct=None,
    distinct_min_frac=None,
):
    """Match two point sets; returns [PointAlignment], best first.

    left, right: (n, d) arrays, d >= 1 ("rt") or d >= 2 ("rts").
    labels: (left, right) per-row labels (any hashable values); only
        equal labels match.
    orientations: (left, right) per-row directions in degrees, for
        2-D "rts" with orientation_tol_deg > 0: a triangle pair is skipped
        unless the circular gaps between its three directions agree
        within the tolerance (rotation-invariant; NaN = no check).
    workers: threads, 0 = all cores.
    progress: callable(fraction, stage) -> bool, called now and then
        from the calling thread; returning True raises Cancelled.
        Ctrl-C interrupts the call (KeyboardInterrupt).
    align2d / clique: scpp INI sections as mappings.

    Options (default):
        mode: "rt" rigid or "rts" similarity ("rt"); [align2d]
            same_scale is the same choice (True = "rt").
        epsilon: allowed relative distance error (0.005).
        scale_range: "rts" scales within [1/s, s] (5).
        anchor_tol: anchored search; 0 = off. 2-D: rejects mirror
            images; "rts" in d > 2: every match shares one scale
            (mirror images kept), and needed for speed as d grows
            (0.05).
        min_support: "rts" edges need this many matched triangles (1).
        orientation_tol_deg: see orientations; [align2d] alias
            minutia_angle_tol_deg (0 = off).
        min_matched: fewer matched pairs is no match (3).
        max_matched: cap on the matched pairs; 0 = fewer of the two
            counts (0).
        results: distinct alignments to return (2).
        distinct: "explained" (later results contain no pair an
            earlier transform explains) or "disjoint" (no point shared
            with an earlier result) ("explained").
        distinct_min_frac: later results need this fraction of the
            first one's size (0.7).
    """
    args, L, R = _point_args(
        left, right, labels, orientations, workers, align2d, clique,
        _CLIQUE,
        dict(
            mode=mode,
            same_scale=same_scale,
            epsilon=epsilon,
            scale_range=scale_range,
            anchor_tol=anchor_tol,
            min_support=min_support,
            orientation_tol_deg=orientation_tol_deg,
            minutia_angle_tol_deg=minutia_angle_tol_deg,
            min_matched=min_matched,
            max_matched=max_matched,
            results=results,
            distinct=distinct,
            distinct_min_frac=distinct_min_frac,
        ),
    )
    status, message, out = _core.align_points(progress=progress, **args)
    return _finish(status, message, out, PointAlignment)


def match_points(
    left,
    right,
    *,
    labels=None,
    orientations=None,
    workers=0,
    progress=None,
    align2d=None,
    clique=None,
    mode=None,
    same_scale=None,
    epsilon=None,
    scale_range=None,
    anchor_tol=None,
    min_support=None,
    orientation_tol_deg=None,
    minutia_angle_tol_deg=None,
    min_matched=None,
    max_matched=None,
    results=None,
    distinct=None,
    distinct_min_frac=None,
):
    """align_points without fitting a transform: the matched points of
    each distinct match, best first; returns [PointMatch], [] when
    nothing matches.

    Takes the same arguments and options as align_points. Later
    matches share no point with earlier ones (distinct="disjoint", the
    default here; "explained" needs a transform and raises). Without a
    fit there is no check of the overall scale: for "rts", scale_range
    only bounds each matched triangle pair.
    """
    args, L, R = _point_args(
        left, right, labels, orientations, workers, align2d, clique,
        _CLIQUE_MATCH,
        dict(
            mode=mode,
            same_scale=same_scale,
            epsilon=epsilon,
            scale_range=scale_range,
            anchor_tol=anchor_tol,
            min_support=min_support,
            orientation_tol_deg=orientation_tol_deg,
            minutia_angle_tol_deg=minutia_angle_tol_deg,
            min_matched=min_matched,
            max_matched=max_matched,
            results=results,
            distinct=distinct,
            distinct_min_frac=distinct_min_frac,
        ),
    )
    if not args.pop("disjoint"):
        raise ValueError(
            "match_points fits no transform, so distinct='explained' "
            "is not available; use 'disjoint'"
        )
    status, message, out = _core.match_points(progress=progress, **args)
    return _finish(
        status,
        message,
        [dict(pairs=p) for p in out],
        lambda pairs: PointMatch(pairs, L[pairs[:, 0]], R[pairs[:, 1]]),
    )


def align_lines_3d(
    left,
    right,
    *,
    labels=None,
    workers=0,
    progress=None,
    align3d=None,
    clique=None,
    angle_tol_deg=None,
    sep_tol=None,
    sep_tol_um=None,
    anchor_tol=None,
    anchor_tol_um=None,
    extent_scale=None,
    max_length_ratio=None,
    max_rotation_deg=None,
    min_matched=None,
    max_matched=None,
    results=None,
    distinct=None,
    distinct_min_frac=None,
):
    """Rigid matching of 3-D line segments; returns [LineAlignment].

    left, right: (n, 7) arrays (midpoint xyz, direction xyz, length) or
        (n, 6) without lengths (taken as 0). Directions may have any
        length and sign.
    labels, workers, progress, clique, and the clique options: as in
        align_points. align3d: scpp's [align3d] section as a mapping.

    Options (default):
        angle_tol_deg: allowed angle difference of line pairs; 0 =
            no angle check (5).
        sep_tol: allowed separation difference, position units; or
            sep_tol_um for positions in metres (5 um, i.e. 5e-6).
        anchor_tol: anchored search, every line must fit the transform
            of its match's first two lines within this depth (across:
            sep_tol), position units; or anchor_tol_um for positions
            in metres; 0 = off (1 um, i.e. 1e-6).
        extent_scale: widen the separation tolerance for segments
            whose lengths differ; 0 = off (0).
        max_length_ratio, max_rotation_deg: scpp-only rules, accepted
            as 0 (off) only.
    """
    o = _merge(
        [("align3d", align3d, _LINES), ("clique", clique, _CLIQUE)],
        dict(
            angle_tol_deg=angle_tol_deg,
            sep_tol=sep_tol,
            sep_tol_um=sep_tol_um,
            anchor_tol=anchor_tol,
            anchor_tol_um=anchor_tol_um,
            extent_scale=extent_scale,
            max_length_ratio=max_length_ratio,
            max_rotation_deg=max_rotation_deg,
            min_matched=min_matched,
            max_matched=max_matched,
            results=results,
            distinct=distinct,
            distinct_min_frac=distinct_min_frac,
        ),
    )
    for key in ("max_length_ratio", "max_rotation_deg"):
        if o[key] != 0:
            raise ValueError(
                f"{key} is an scpp rule, not part of aligncalc; pass 0"
            )
    sep = _metres(o, "sep_tol", 5.0)
    anchor = _metres(o, "anchor_tol", 1.0)
    if not o["angle_tol_deg"] >= 0:
        raise ValueError("angle_tol_deg must be >= 0")
    if not sep > 0:
        raise ValueError("sep_tol must be > 0")
    if not anchor >= 0:
        raise ValueError("anchor_tol must be >= 0")
    if not o["extent_scale"] >= 0:
        raise ValueError("extent_scale must be >= 0")
    c = _clique_args(o, workers)

    L = _rows("left", left, (6, 7))
    R = _rows("right", right, (6, 7))
    if L.shape[1] == 6:
        L = np.hstack([L, np.zeros((len(L), 1))])
    if R.shape[1] == 6:
        R = np.hstack([R, np.zeros((len(R), 1))])
    for name, m in (("left", L), ("right", R)):
        if np.any(np.linalg.norm(m[:, 3:6], axis=1) == 0):
            raise ValueError(f"{name} has a zero direction")
    lab = _pair("labels", labels, len(L), len(R))
    label_col = -1
    if lab is not None:
        a, b = _label_codes(lab)
        L = np.hstack([L, a[:, None]])
        R = np.hstack([R, b[:, None]])
        label_col = 7
    status, message, out = _core.align_lines_3d(
        np.ascontiguousarray(L),
        np.ascontiguousarray(R),
        label_col=label_col,
        angle_tol=math.radians(o["angle_tol_deg"]),
        sep_tol=sep,
        anchor_tol=anchor,
        extent_scale=o["extent_scale"],
        progress=progress,
        **c,
    )
    return _finish(status, message, out, LineAlignment)
