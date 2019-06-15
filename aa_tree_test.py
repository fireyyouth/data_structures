from aa_tree import AATree
import unittest
import random

class AATreeTest(unittest.TestCase):
    def test_normal(self):
        t = AATree()
        d = {}

        # test AATree.set()
        for i in range(1000):
            k = random.random()
            v = random.random()
            d[k] = v
            t.set(k, v)
        self.assertEqual(len(d), len(t))
        self.assertEqual(sorted(d.keys()), t.keys())
        for k in d:
            self.assertEqual(d[k], t.get(k))

        # test AATree.set() same key more then once
        for k in d.keys():
            v = random.random()
            d[k] = v
            t.set(k, v)
        self.assertEqual(len(d), len(t))
        self.assertEqual(sorted(d.keys()), t.keys())
        for k in d:
            self.assertEqual(d[k], t.get(k))

        # test remove
        n = len(d)     
        while len(d) > n // 2:
            k, _ = d.popitem()
            t.remove(k)
        self.assertEqual(len(d), len(t))
        self.assertEqual(sorted(d.keys()), t.keys())
        for k in d:
            self.assertEqual(d[k], t.get(k))

    def test_exception(self):
        t = AATree()
        t.set(0, 0)

        self.assertEqual(t.get(0), 0)
        with self.assertRaises(KeyError):
            t.get(1)

        with self.assertRaises(KeyError):
            t.remove(1)
        t.remove(0)


if __name__ == '__main__':
    unittest.main()
