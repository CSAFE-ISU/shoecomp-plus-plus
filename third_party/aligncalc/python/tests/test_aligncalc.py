"""Smoke tests on synthetic data: pytest, or run this file directly."""

import configparser
import math
import threading
import time
import _thread

import numpy as np

import aligncalc


def rot2(theta):
    c, s = math.cos(theta), math.sin(theta)
    return np.array([[c, -s], [s, c]])


def similarity_case(seed=1, n=30, extra=10, scale=1.3, theta=0.7, t=(40, -70)):
    """right = scale * R(theta) left + t, rows shuffled, plus extras.
    Returns left, right, truth (right row of each left row)."""
    rng = np.random.default_rng(seed)
    left = rng.uniform(0, 1000, (n, 2))
    moved = scale * left @ rot2(theta).T + t
    right = np.vstack([moved, rng.uniform(0, 1000, (extra, 2))])
    perm = rng.permutation(len(right))
    out = np.empty_like(right)
    out[perm] = right
    return left, out, perm[:n]


def correct(a, truth):
    return int(np.sum(truth[a.pairs[:, 0]] == a.pairs[:, 1]))


def lines_case(seed=2, n=30, extra=8):
    """Nearly parallel 3-D segments (metres), right = rigid copy."""
    rng = np.random.default_rng(seed)
    axis = np.array([0.2, -0.1, 1.0]) / np.linalg.norm([0.2, -0.1, 1.0])
    k = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]],
                  [-axis[1], axis[0], 0]])
    rot = np.eye(3) + math.sin(0.3) * k + (1 - math.cos(0.3)) * k @ k
    t = np.array([1e-4, 2e-4, -5e-5])

    def segments(m):
        mid = rng.uniform(0, 2e-3, (m, 3)) * [1, 1, 0.1]
        d = np.column_stack([np.ones(m), rng.uniform(-0.02, 0.02, m),
                             rng.uniform(-0.01, 0.01, m)])
        return mid, d / np.linalg.norm(d, axis=1)[:, None]

    mid, d = segments(n)
    length = rng.uniform(3e-4, 7e-4, n)
    left = np.column_stack([mid, d, length])
    flip = np.where(np.arange(n) % 2, -1.0, 1.0)[:, None]
    right = np.column_stack([mid @ rot.T + t, flip * (d @ rot.T), length])
    m2, d2 = segments(extra)
    left = np.vstack([left, np.column_stack([m2, d2, np.full(extra, 5e-4)])])
    m3, d3 = segments(extra)
    right = np.vstack([right, np.column_stack([m3 @ rot.T + t, d3 @ rot.T,
                                               np.full(extra, 5e-4)])])
    return left, right, rot, t


# ---- points ----------------------------------------------------------


def test_similarity_recovered():
    left, right, truth = similarity_case()
    res = aligncalc.align_points(left, right, mode="rts")
    assert res and isinstance(res[0], aligncalc.PointAlignment)
    a = res[0]
    assert a.pairs.dtype == np.int32 and a.pairs.shape == (30, 2)
    assert correct(a, truth) == 30
    assert abs(a.scale - 1.3) < 1e-6 and abs(a.angle - 0.7) < 1e-6
    assert np.allclose(a.translation, [40, -70], atol=1e-6)
    assert a.explained >= len(a.pairs)
    assert np.allclose(a.apply(left[a.pairs[:, 0]]), right[a.pairs[:, 1]])


def test_rigid_default_misses_scaled_copy():
    left, right, truth = similarity_case()
    res = aligncalc.align_points(left, right)  # "rt"
    assert all(correct(a, truth) < 10 for a in res)
    left, right, truth = similarity_case(scale=1.0)
    a = aligncalc.align_points(left, right)[0]
    assert correct(a, truth) == 30 and a.scale == 1.0


def test_rigid_1d_3d():
    rng = np.random.default_rng(3)
    p = rng.uniform(-50, 50, (25, 3))
    q, _ = np.linalg.qr(rng.normal(size=(3, 3)))
    if np.linalg.det(q) < 0:
        q[:, 0] = -q[:, 0]
    right = np.vstack([p @ q.T + [5, -3, 2], rng.uniform(-50, 50, (8, 3))])
    a = aligncalc.align_points(p, right, epsilon=0.01, min_matched=5)[0]
    assert np.sum(a.pairs[:, 0] == a.pairs[:, 1]) == 25
    assert np.allclose(a.rotation, q, atol=1e-6)
    assert np.allclose(a.translation, [5, -3, 2], atol=1e-6)
    # 1-D, given as a flat array
    x = rng.uniform(-50, 50, 25)
    a = aligncalc.align_points(x, np.concatenate([x + 7, [100, 200]]),
                               epsilon=0.01, min_matched=5)[0]
    assert a.rotation.shape == (1, 1) and abs(a.translation[0] - 7) < 1e-9


def test_labels_restrict_matches():
    left, right, truth = similarity_case()
    lab_l = np.array(["a", "b", "c"])[np.arange(30) % 3]
    lab_r = np.full(len(right), "x", dtype=object)
    lab_r[truth] = lab_l
    a = aligncalc.align_points(left, right, mode="rts",
                               labels=(lab_l, lab_r))[0]
    assert correct(a, truth) == 30
    assert np.all(lab_l[a.pairs[:, 0]] == lab_r[a.pairs[:, 1]])
    # shifted labels: no true pair is allowed
    shifted = lab_r.copy()
    shifted[truth] = np.roll(lab_l, 1)
    res = aligncalc.align_points(left, right, mode="rts",
                                 labels=(lab_l, shifted))
    assert all(correct(a, truth) == 0 for a in res)


def test_orientations():
    left, right, truth = similarity_case()
    ori_l = np.linspace(0, 300, 30)
    ori_r = np.zeros(len(right))
    ori_r[truth] = ori_l + math.degrees(0.7)
    a = aligncalc.align_points(left, right, mode="rts",
                               orientations=(ori_l, ori_r),
                               orientation_tol_deg=10)[0]
    assert correct(a, truth) == 30
    # a constant offset is a rotation: still found
    ori_r[truth] = ori_l + 90
    a = aligncalc.align_points(left, right, mode="rts",
                               orientations=(ori_l, ori_r),
                               minutia_angle_tol_deg=10)[0]
    assert correct(a, truth) == 30
    # unrelated orientations: the copy is not found
    ori_r[truth] = np.random.default_rng(9).uniform(0, 360, 30)
    res = aligncalc.align_points(left, right, mode="rts",
                                 orientations=(ori_l, ori_r),
                                 minutia_angle_tol_deg=10)
    assert all(correct(a, truth) < 10 for a in res)


def test_two_copies_distinct():
    rng = np.random.default_rng(4)
    left = rng.uniform(0, 1000, (40, 2))
    right = np.vstack([left[:20] @ rot2(0.4).T + [1200, -300],
                       left[20:36] @ rot2(-2.1).T + [-800, 1500]])
    for distinct in ("explained", "disjoint"):
        res = aligncalc.align_points(left, right, results=3,
                                     distinct=distinct)
        assert len(res) >= 2
        assert sorted(res[0].pairs[:, 0]) == list(range(20))
        assert sorted(res[1].pairs[:, 0]) == list(range(20, 36))
        assert np.all(res[1].pairs[:, 1] == res[1].pairs[:, 0])


def test_workers_do_not_change_results():
    left, right, _ = similarity_case(seed=5, extra=30)
    r1 = aligncalc.align_points(left, right, mode="rts", results=3,
                                workers=1)
    r4 = aligncalc.align_points(left, right, mode="rts", results=3,
                                workers=4)
    assert len(r1) == len(r4)
    for a, b in zip(r1, r4):
        assert np.array_equal(a.pairs, b.pairs)
        assert np.array_equal(a.rotation, b.rotation)


def test_no_match_is_empty():
    rng = np.random.default_rng(6)
    res = aligncalc.align_points(rng.uniform(0, 1000, (20, 2)),
                                 rng.uniform(0, 1000, (20, 2)),
                                 min_matched=15)
    assert res == []
    assert aligncalc.align_points(np.zeros((3, 2)), np.ones((4, 2)),
                                  min_matched=5) == []


def test_errors():
    left, right, _ = similarity_case()
    bad = [
        dict(mode="rts", left=np.zeros((5, 3)), right=np.zeros((5, 3))),
        dict(left=np.zeros((5, 4)), right=np.zeros((5, 4))),
        dict(left=np.zeros((5, 2)), right=np.zeros((5, 3))),
        dict(epsilon=0),
        dict(scale_range=0.5),
        dict(distinct="neither"),
        dict(mode="rtx"),
        dict(mode="rt", same_scale=False),
        dict(labels=(np.zeros(30), None)),
        dict(labels=(np.zeros(29), np.zeros(40))),
    ]
    for kw in bad:
        kw = dict(kw)
        lft, rgt = kw.pop("left", left), kw.pop("right", right)
        try:
            aligncalc.align_points(lft, rgt, **kw)
        except (ValueError, TypeError):
            continue
        raise AssertionError(f"no error for {kw}")


# ---- match_points ----------------------------------------------------


def test_match_points_equals_align_disjoint():
    for scale, mode in ((1.3, "rts"), (1.0, "rt")):
        left, right, truth = similarity_case(seed=10, extra=20,
                                             scale=scale)
        m = aligncalc.match_points(left, right, mode=mode, results=3)
        a = aligncalc.align_points(left, right, mode=mode, results=3,
                                   distinct="disjoint")
        assert m and isinstance(m[0], aligncalc.PointMatch)
        assert len(m) == len(a)
        for x, y in zip(m, a):
            assert np.array_equal(x.pairs, y.pairs)
        best = m[0]
        assert correct(best, truth) == 30
        assert np.array_equal(best.left_points, left[best.pairs[:, 0]])
        assert np.array_equal(best.right_points, right[best.pairs[:, 1]])
        assert best.left_points.shape == (30, 2)


def test_match_points_two_copies_and_options():
    rng = np.random.default_rng(4)
    left = rng.uniform(0, 1000, (40, 2))
    right = np.vstack([left[:20] @ rot2(0.4).T + [1200, -300],
                       left[20:36] @ rot2(-2.1).T + [-800, 1500]])
    m = aligncalc.match_points(left, right, results=3,
                               clique={"min_matched": "5"})
    assert sorted(m[0].pairs[:, 0]) == list(range(20))
    assert sorted(m[1].pairs[:, 0]) == list(range(20, 36))
    used = np.concatenate([x.pairs for x in m])
    assert len(set(used[:, 0])) == len(used) == len(set(used[:, 1]))
    assert aligncalc.match_points(left, right, distinct="disjoint")
    for kw in (dict(distinct="explained"),
               dict(clique={"distinct": "explained"}),
               dict(mode="rts", left=np.zeros((5, 3)))):
        lft = kw.pop("left", left)
        try:
            aligncalc.match_points(lft, right if lft is left else lft,
                                   **kw)
        except ValueError:
            continue
        raise AssertionError(f"no error for {kw}")
    assert aligncalc.match_points(rng.uniform(0, 1000, (20, 2)),
                                  rng.uniform(0, 1000, (20, 2)),
                                  min_matched=15) == []
    try:
        aligncalc.match_points(left, right, progress=lambda f, s: True)
    except aligncalc.Cancelled:
        pass
    else:
        raise AssertionError("not cancelled")


# ---- INI-style options -----------------------------------------------

INI = """
[align2d]
scale_range = 3
epsilon = 0.005
same_scale = false
anchor_tol = 0.05
minutia_angle_tol_deg = 20

[align3d]
angle_tol_deg = 5
sep_tol_um = 5
max_length_ratio = 0
extent_scale = 0
max_rotation_deg = 0
anchor_tol_um = 1

[clique]
min_matched = 5
max_matched = 0
results = 3
distinct = explained
"""


def test_ini_sections_equal_keywords():
    cfg = configparser.ConfigParser()
    cfg.read_string(INI)
    left, right, _ = similarity_case(seed=7, extra=20)
    a = aligncalc.align_points(left, right, align2d=cfg["align2d"],
                               clique=cfg["clique"])
    b = aligncalc.align_points(left, right, mode="rts", scale_range=3,
                               min_matched=5, results=3)
    c = aligncalc.align_points(left, right, clique=dict(cfg["clique"]),
                               **dict(cfg["align2d"]))
    assert len(a) == len(b) == len(c) >= 1
    for x, y, z in zip(a, b, c):
        assert np.array_equal(x.pairs, y.pairs)
        assert np.array_equal(x.pairs, z.pairs)
    lft, rgt, _, _ = lines_case()
    r = aligncalc.align_lines_3d(lft, rgt, align3d=cfg["align3d"],
                                 clique=cfg["clique"])
    assert r and len(r[0].pairs) >= 28


def test_ini_key_errors():
    left, right, _ = similarity_case()
    for kw in [
        dict(clique={"min_matches": 5}),  # typo
        dict(clique={"min_matched": 5}, min_matched=6),  # twice
        dict(align2d={"results": 2}),  # wrong section
        dict(clique={"min_matched": "five"}),
        dict(align2d={"same_scale": "maybe"}),
        dict(orientation_tol_deg=5, minutia_angle_tol_deg=5),
        dict(not_an_option=1),
    ]:
        try:
            aligncalc.align_points(left, right, **kw)
        except (TypeError, ValueError):
            continue
        raise AssertionError(f"no error for {kw}")


# ---- lines -----------------------------------------------------------


def test_lines_recovered():
    left, right, rot, t = lines_case()
    res = aligncalc.align_lines_3d(left, right, angle_tol_deg=0.6,
                                   sep_tol=2e-6, min_matched=5)
    assert res and isinstance(res[0], aligncalc.LineAlignment)
    a = res[0]
    assert np.sum(a.pairs[:, 0] == a.pairs[:, 1]) >= 28
    assert np.allclose(a.rotation, rot, atol=1e-6)
    assert np.allclose(a.translation, t, atol=1e-8)
    assert np.allclose(a.matrix[:3, 3], t, atol=1e-8)
    assert a.apply(left[:3, :3]).shape == (3, 3)
    # without lengths
    res6 = aligncalc.align_lines_3d(left[:, :6], right[:, :6],
                                    angle_tol_deg=0.6, sep_tol=2e-6,
                                    min_matched=5)
    assert np.array_equal(res6[0].pairs, a.pairs)


def test_lines_labels_and_scpp_rules():
    left, right, _, _ = lines_case()
    lab = np.arange(len(left)) % 3
    res = aligncalc.align_lines_3d(left, right, labels=(lab, lab),
                                   min_matched=5, sep_tol=2e-6)
    assert res and np.all(lab[res[0].pairs[:, 0]] == lab[res[0].pairs[:, 1]])
    res = aligncalc.align_lines_3d(left, right,
                                   labels=(lab, (lab + 1) % 3),
                                   min_matched=5, sep_tol=2e-6)
    assert all(np.sum(a.pairs[:, 0] == a.pairs[:, 1]) == 0 for a in res)
    for kw in (dict(max_rotation_deg=30), dict(max_length_ratio=2),
               dict(sep_tol=1e-6, sep_tol_um=1),
               dict(anchor_tol=1e-6, anchor_tol_um=1),
               dict(anchor_tol=-1e-6)):
        try:
            aligncalc.align_lines_3d(left, right, **kw)
        except (ValueError, TypeError):
            continue
        raise AssertionError(f"no error for {kw}")


def test_lines_anchor():
    left, right, _, _ = lines_case()
    # default (anchored, 1 um), the same in metres, and off
    runs = [aligncalc.align_lines_3d(left, right, sep_tol=2e-6,
                                     min_matched=5, **kw)
            for kw in (dict(), dict(anchor_tol=1e-6),
                       dict(anchor_tol_um=1), dict(anchor_tol=0))]
    for res in runs:
        assert res and np.sum(res[0].pairs[:, 0] == res[0].pairs[:, 1]) >= 28
    assert np.array_equal(runs[0][0].pairs, runs[1][0].pairs)
    assert np.array_equal(runs[0][0].pairs, runs[2][0].pairs)


# ---- progress and cancellation ---------------------------------------


def big_case():
    # about 1 s
    return similarity_case(seed=8, n=40, extra=40)


def test_progress_reports():
    seen = []
    caller = threading.get_ident()

    def progress(fraction, stage):
        seen.append((fraction, stage, threading.get_ident()))

    left, right, _ = similarity_case()
    aligncalc.align_points(left, right, mode="rts", progress=progress)
    assert len(seen) >= 3
    fractions = [f for f, _, _ in seen]
    assert fractions == sorted(fractions) and 0 <= fractions[0]
    assert fractions[-1] <= 1
    assert all(isinstance(s, str) and s for _, s, _ in seen)
    assert all(t == caller for _, _, t in seen)


def test_progress_cancel_and_exception():
    left, right, _ = similarity_case()
    try:
        aligncalc.align_points(left, right, mode="rts",
                               progress=lambda f, s: f >= 0.3)
    except aligncalc.Cancelled:
        pass
    else:
        raise AssertionError("not cancelled")

    def boom(f, s):
        raise RuntimeError("from the callback")

    try:
        aligncalc.align_points(left, right, mode="rts", progress=boom)
    except RuntimeError as e:
        assert "callback" in str(e)
    else:
        raise AssertionError("callback exception lost")


def test_keyboard_interrupt():
    left, right, _ = big_case()
    start = time.monotonic()
    full = None
    try:
        aligncalc.align_points(left, right, mode="rts")
        full = time.monotonic() - start
    except KeyboardInterrupt:
        raise AssertionError("interrupted without a signal")
    # interrupt a run after 10% of its uninterrupted time
    timer = threading.Timer(full / 10, _thread.interrupt_main)
    start = time.monotonic()
    timer.start()
    try:
        aligncalc.align_points(left, right, mode="rts")
    except KeyboardInterrupt:
        took = time.monotonic() - start
        assert took < full / 2, (took, full)
    else:
        raise AssertionError(f"not interrupted (run takes {full:.2f} s)")
    finally:
        timer.cancel()


if __name__ == "__main__":
    import sys

    failed = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print("PASS", name)
            except Exception as e:  # noqa: BLE001
                failed += 1
                print("FAIL", name, type(e).__name__, e)
    sys.exit(1 if failed else 0)
