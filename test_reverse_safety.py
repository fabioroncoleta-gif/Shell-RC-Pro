import unittest
from reverse_safety import ReverseUnlock

class ReverseUnlockTest(unittest.TestCase):
    def test_three_full_presses(self):
        r=ReverseUnlock()
        for t in (1,1.6,2.2):
            r.update(0,0,t-.1)
            r.update(.98,0,t)
        self.assertTrue(r.unlocked)

    def test_half_press_never_unlocks(self):
        r=ReverseUnlock()
        for t in (1,1.5,2,2.5,3):
            r.update(0,0,t-.1)
            r.update(.8,0,t)
        self.assertFalse(r.unlocked)

    def test_without_release_no_counting(self):
        r=ReverseUnlock()
        for i in range(50):
            r.update(1,0,i*.02)
        self.assertFalse(r.unlocked)
        self.assertEqual(r.count,1)

    def test_timeout(self):
        r=ReverseUnlock()
        for t in (1,5,9):
            r.update(0,0,t-.1)
            r.update(1,0,t)
        self.assertFalse(r.unlocked)

    def test_reset(self):
        r=ReverseUnlock()
        for t in (1,1.5,2):
            r.update(0,0,t-.1)
            r.update(1,0,t)
        self.assertTrue(r.unlocked)
        r.reset()
        self.assertFalse(r.unlocked)

    def test_cannot_unlock_with_forward_pressed(self):
        r=ReverseUnlock()
        for t in (1,1.5,2):
            r.update(0,0,t-.1)
            r.update(1,.5,t)
        self.assertFalse(r.unlocked)

if __name__=="__main__":
    unittest.main()
