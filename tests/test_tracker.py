"""
test_tracker.py -- valida que a porta Python espelha o C (rode: pytest -q
ou simplesmente: python test_tracker.py). Nao precisa de OpenCV.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "pc_prototype"))
from argus_tracker import Argus, Event, MAX_MISSED, MAX_TRACKS

W, H = 320, 240


def run_single(x0, x1, steps=10):
    a = Argus(W, H)
    for i in range(steps + 1):
        x = x0 + (x1 - x0) * i // steps
        a.update([(x, H // 2)])
    for _ in range(MAX_MISSED + 2):
        a.update([])
    return a.poll_event()


def test_in():
    assert run_single(20, 300) == Event.IN

def test_out():
    assert run_single(300, 20) == Event.OUT

def test_jitter_ambiguous():
    assert run_single(150, 165) == Event.AMBIGUOUS

def test_no_cross_ambiguous():
    assert run_single(20, 140) == Event.AMBIGUOUS

def test_three_objects():
    a = Argus(W, H)
    maxN = 0
    for t in range(11):
        x = 20 + 280 * t // 10
        a.update([(x, 60), (x, 120), (x, 180)])
        maxN = max(maxN, a.n_tracks)
    assert maxN == 3
    for _ in range(MAX_MISSED + 2):
        a.update([])
    evs = []
    while (e := a.poll_event()) != Event.NONE:
        evs.append(e)
    assert len(evs) == 3
    assert all(e == Event.IN for e in evs)

def test_saturation():
    a = Argus(W, H)
    a.update([(10 + k * 15, 120) for k in range(20)])
    assert a.n_tracks <= MAX_TRACKS


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    p = f = 0
    for fn in fns:
        try:
            fn(); p += 1; print(f"  ok  {fn.__name__}")
        except AssertionError:
            f += 1; print(f"  FALHOU {fn.__name__}")
    print(f"\n== {p} passou, {f} falhou ==")
    sys.exit(1 if f else 0)
