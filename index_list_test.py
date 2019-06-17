from index_list import IndexList
import unittest

class IndexListTest(unittest.TestCase):
    def test_normal(self):
        a = IndexList()
        b = []
        for i in range(1000):
            a.insert(len(a), i)
            b.append(i)
        
        self.assertEqual(len(a), len(b))
        for i in range(len(a)):
            self.assertEqual(a.get(i), b[i])

        a.insert(2, -1)
        b.insert(2, -1)

        self.assertEqual(len(a), len(b))
        for i in range(len(a)):
            self.assertEqual(a.get(i), b[i])

        a.remove(5)
        b.pop(5)

        self.assertEqual(len(a), len(b))
        for i in range(len(a)):
            self.assertEqual(a.get(i), b[i])

    def test_exception(self):
        a = IndexList()

        with self.assertRaises(IndexError):
            a.get(0)

        with self.assertRaises(IndexError):
            a.insert(1, 0)

        a.insert(0, 0)
        self.assertEqual(a.get(0), 0)

        with self.assertRaises(IndexError):
            a.remove(1)

        a.remove(0)
    
    def test_iteration(self):
        a = IndexList()
        b = []
        for i in range(1000):
            a.insert(len(a), i)
            b.append(i)

        for x, y in zip(a, b):
            self.assertEqual(x, y)


unittest.main()
