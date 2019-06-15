class Node:
    def __init__(self, key, value, left, right, level):
        self.key = key
        self.value = value
        self._right = right
        self._left = left
        self.level = level
        self.size = 1

    def _update_size(self):
        self.size = 1 + self._left.size + self._right.size

    def set_left(self, left):
        self._left = left
        self._update_size()

    def set_right(self, right):
        self._right = right
        self._update_size()

    def left(self):
        return self._left

    def right(self):
        return self._right

Node.sentinel = Node(None, None, None, None, 0)
Node.sentinel.size = 0

class AATree:
    def __init__(self):
        self._root = Node.sentinel

    def _right_rotate(self, node):
        t = node.left()
        node.set_left(t.right())
        t.set_right(node)
        return t

    def _left_rotate(self, node):
        t = node.right()
        node.set_right(t.left())
        t.set_left(node)
        return t
    
    def _skew(self, node):
        assert type(node) == Node and node.level > 0
        if node.left().level == node.level:
            node = self._right_rotate(node)
        return node
    
    def _split(self, node):
        assert type(node) == Node and node.level > 0
        if node.right().right() != None and node.level == node.right().level == node.right().right().level:
            node = self._left_rotate(node)     
            node.level += 1
        return node
   
    def _set(self, node, key, value):
        if node.level == 0:
            return Node(key, value, Node.sentinel, Node.sentinel, 1)

        if key == node.key:
            node.value = value 
            return node

        if key < node.key:
            node.set_left(self._set(node.left(), key, value))
        else:
            node.set_right(self._set(node.right(), key, value))
        
        node = self._skew(node)
        node = self._split(node)
        
        return node
    
    def _succecor(self, node):
        node = node.right()
        while node.left().level > 0:
            node = node.left()
        return node
    
    def _swap(self, a, b):
        a.value, b.value = b.value, a.value
        a.key, b.key = b.key, a.key

    def _remove(self, node, key):
        if node.level == 0:
            raise KeyError("%s not found" % key)
        
        kid = -1

        if key == node.key:
            if node.right().level == 0:
                return Node.sentinel
            else:
                t = self._succecor(node)
                self._swap(t, node)
                kid = 1
         
        if kid == -1:
            kid = 0 if key < node.key else 1

        if kid == 0:
            node.set_left(self._remove(node.left(), key))
            if node.left().level < node.level - 1:
                assert node.left().level == node.level - 2
                if node.right().level < node.level:
                    node.level -= 1
                    node = self._split(node)
                else:
                    node.level -= 1
                    node.right().level -= 1
                    node.set_right(self._skew(node.right()))
                    node.right().set_right(self._skew(node.right().right()))
                    node = self._split(node)
                    node.set_right(self._split(node.right()))
        else:
            node.set_right(self._remove(node.right(), key))
            if node.right().level < node.level - 1:
                assert node.right().level == node.level - 2
                node.level -= 1
                node = self._skew(node)
                node.set_right(self._skew(node.right()))
                node = self._split(node)
        
        return node 

    def set(self, key, value):
        self._root = self._set(self._root, key, value)
    
    def remove(self, key):
        self._root = self._remove(self._root, key)
    
    def _show(self, node, s):
        if node.level > 0:
            self._show(node.right(), s + 1)
            print(s * '\t', (node.key, node.level))
            self._show(node.left(), s + 1)

    def _keys(self, node):
        if node.level == 0:
            return []
        else:
            return self._keys(node.left()) + [node.key] + self._keys(node.right())
    
    def keys(self):
        return self._keys(self._root)

    def show(self):
        self._show(self._root, 0)
    
    def get(self, key):
        node = self._root
        while node != Node.sentinel:
            if key == node.key:
                return node.value
            elif key < node.key:
                node = node.left()
            else:
                node = node.right()
        raise KeyError("%s not found" % key)
    
    def __len__(self):
        return self._root.size
