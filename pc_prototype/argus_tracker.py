"""
argus_tracker.py -- Porta em Python da maquina de estados ARGUS.

Espelha argus_fsm.c EXATAMENTE (mesma regra IN/OUT/AMBIGUOUS, mesmos
limiares). Nao depende de OpenCV, entao pode ser testado headless e
serve de "verdade de referencia" para o firmware em C.

Use isto para validar a LOGICA rapido no PC; o argus_pc.py so adiciona
a captura de video por cima.
"""
from dataclasses import dataclass, field
from enum import Enum
from typing import List, Optional


class Event(Enum):
    NONE = 0
    IN = 1          # bip simples
    OUT = 2         # bip simples
    AMBIGUOUS = 3   # bip duplo


class State(Enum):
    IDLE = 0
    TRACKING = 1
    DECIDING = 2
    FEEDBACK = 3


# --- parametros (iguais ao argus_fsm.h) ---
MAX_TRACKS = 8
MAX_MISSED = 3
GATE_PCT = 25
MIN_TRAVEL_PCT = 20
MAX_AGE = 60


@dataclass
class Track:
    id: int
    start_x: int
    last_x: int
    last_y: int
    age: int = 1
    missed: int = 0
    active: bool = True


@dataclass
class Argus:
    w: int
    h: int
    center_x: int = 0
    next_id: int = 1
    tracks: List[Track] = field(default_factory=list)
    events: List[Event] = field(default_factory=list)
    state: State = State.IDLE

    def __post_init__(self):
        self.center_x = self.w // 2

    @property
    def n_tracks(self) -> int:
        return sum(1 for t in self.tracks if t.active)

    def _classify(self, t: Track) -> Event:
        travel = abs(t.last_x - t.start_x)
        if travel < (self.w * MIN_TRAVEL_PCT) // 100:
            return Event.AMBIGUOUS
        started_door = t.start_x < self.center_x
        ended_interior = t.last_x > self.center_x
        started_interior = t.start_x > self.center_x
        ended_door = t.last_x < self.center_x
        if started_door and ended_interior:
            return Event.IN
        if started_interior and ended_door:
            return Event.OUT
        return Event.AMBIGUOUS

    def update(self, dets: List[tuple]) -> State:
        gate = (self.w * GATE_PCT) // 100
        used = [False] * len(dets)

        # 1. casamento guloso vizinho-mais-proximo (L1)
        for t in self.tracks:
            if not t.active:
                continue
            best, best_dist = -1, gate + 1
            for d, (dx, dy) in enumerate(dets):
                if used[d]:
                    continue
                dist = abs(dx - t.last_x) + abs(dy - t.last_y)
                if dist <= gate and dist < best_dist:
                    best_dist, best = dist, d
            if best >= 0:
                t.last_x, t.last_y = dets[best]
                t.missed = 0
                t.age += 1
                used[best] = True
            else:
                t.missed += 1
                t.age += 1

        # 2. finalizar / declarar perda
        for t in self.tracks:
            if not t.active:
                continue
            if t.age > MAX_AGE:
                self.events.append(Event.AMBIGUOUS)
                t.active = False
            elif t.missed > MAX_MISSED:
                self.events.append(self._classify(t))
                t.active = False

        # 3. deteccoes novas -> tracks novos (respeitando MAX_TRACKS)
        for d, (dx, dy) in enumerate(dets):
            if used[d]:
                continue
            if self.n_tracks >= MAX_TRACKS:
                break
            self.tracks.append(Track(self.next_id, dx, dx, dy))
            self.next_id += 1

        # limpa tracks inativos
        self.tracks = [t for t in self.tracks if t.active]

        # 4. estado
        if self.events:
            self.state = State.DECIDING if self.n_tracks else State.FEEDBACK
        elif self.n_tracks:
            self.state = State.TRACKING
        else:
            self.state = State.IDLE
        return self.state

    def poll_event(self) -> Event:
        return self.events.pop(0) if self.events else Event.NONE
