"""Deteccao de tres acionamentos completos de LT para liberar re total."""
from collections import deque

class ReverseUnlock:
    def __init__(self, full_threshold=.94, release_threshold=.18, window_s=3.0, count=3):
        self.full_threshold = float(full_threshold)
        self.release_threshold = float(release_threshold)
        self.window_s = float(window_s)
        self.required_count = int(count)
        self.press_times = deque(maxlen=self.required_count)
        self.pressed = False
        self.unlocked = False

    def reset(self):
        self.press_times.clear()
        self.pressed = False
        self.unlocked = False

    def update(self, lt, rt, now, armed=True, brake=False):
        if not armed or brake or rt > self.release_threshold:
            self.press_times.clear()
            self.pressed = lt >= self.full_threshold
            return False
        if lt <= self.release_threshold:
            self.pressed = False
        elif lt >= self.full_threshold and not self.pressed:
            self.pressed = True
            if self.press_times and now-self.press_times[0] > self.window_s:
                self.press_times.clear()
            self.press_times.append(now)
            if len(self.press_times) >= self.required_count:
                self.unlocked = True
                self.press_times.clear()
                return True
        return False

    @property
    def count(self):
        return len(self.press_times)
