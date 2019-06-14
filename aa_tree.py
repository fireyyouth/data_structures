class Node:
    sentinel = None
    def __init__(self, key, value):
        self.key = key
        self.value = value
        self.right = Node.sentinel
        self.left = Node.sentinel
        self.level = 1

Node.sentinel = Node(None, None)
Node.sentinel.level = 0

class AATree:
    def __init__(self):
        self._root = Node.sentinel

    def _right_rotate(self, node):
        t = node.left
        node.left = t.right
        t.right = node
        return t

    def _left_rotate(self, node):
        t = node.right
        node.right = t.left
        t.left = node
        return t
    
    def _skew(self, node):
        assert type(node) == Node and node is not Node.sentinel
        if node.left.level == node.level:
            node = self._right_rotate(node)
        return node
    
    def _split(self, node):
        assert type(node) == Node and node is not Node.sentinel
        if node.right.right != None and node.level == node.right.level == node.right.right.level:
            node = self._left_rotate(node)     
            node.level += 1
        return node
   
    def _set(self, node, key, value):
        if node is Node.sentinel:
            return Node(key, value)

        if key == node.key:
            node.value = value 
            return node

        if key < node.key:
            node.left = self._set(node.left, key, value)
        else:
            node.right = self._set(node.right, key, value)
        
        node = self._skew(node)
        node = self._split(node)
        
        return node
    
    def _succecor(self, node):
        node = node.right
        while node.left is not Node.sentinel:
            node = node.left
        return node
    
    def _swap(self, a, b):
        a.value, b.value = b.value, a.value
        a.key, b.key = b.key, a.key

    def _remove(self, node, key):
        if node is Node.sentinel:
            raise KeyError()

        if key == node.key:
            if node.right is not Node.sentinel:
                t = self._succecor(node)
                self._swap(t, node)
                node.right = self._set(node.right, key, value)
                return node
            else:
                return node.right
        
        assert node.level >= 2

        if key < node.key:
            node.left = self._set(node.left, key, value)
            if node.left.level < node.level - 1:
                assert node.left.level == node.level - 2

                node.level -= 1
                if node.right.level == node.level:
                    node = self._split(node)
                else:
                    assert node.right.level == node.level + 1
                    node.right.level -= 1
                    node.right = self._skew(node.right)
                    node.right.right = self._skew(node.right.right)
                    node = self._split(node)
                    node.right = self._split(node.right)
        else:
            node.right = self._set(node.right, key, value)
            if node.right.level < node.level - 1:
                assert node.right.level == node.level - 2
                node.level -= 1
                node = self._skew(node)
        
        return node 

    def set(self, key, value):
        self._root = self._set(self._root, key, value)
    
    def remove(self, key):
        self._root = self._remove(self._root, key)
    
    def _show(self, node, s):
        if node is not Node.sentinel:
            self._show(node.right, s + 1)
            print(s * '\t', (node.key, node.value))
            self._show(node.left, s + 1)

    def show(self):
        self._show(self._root, 0)

    def get(self, key):
        node = self._root
        while node != Node.sentinel:
            if key == node.key:
                return node.value
            elif key < node.key:
                node = node.left
            else:
                node = node.right
        raise KeyError("%s not found" % key)

if __name__ == '__main__':
    from random import random
    t = AATree()
    d = {}
    for i in range(1000):
        key = random()
        value = random()
        d[key] = value
        t.set(key, value)
    
    print(len(d))

    for key in d:
        assert d[key] == t.get(key)
    
    while len(d) > 0:
        k, _ = d.popitem()
        t.remove(k)
        for key in d:
            assert d[key] == t.get(key)
        '''
        print('remove', k)
        print('-' * 50)
        t.show()
        '''


