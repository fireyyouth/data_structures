class Node:
    def __init__(self, value, left, right, level):
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

Node.sentinel = Node(None, None, None, 0)
Node.sentinel.size = 0

class TreeIter:
    def __init__(self, node):
        self.stack = [(node, 0)]
        self.prev = -1
    
    def __next__(self):
        while True:
            if self.stack[-1][0].level == 0 or self.prev == 1:
                node, edge = self.stack.pop()
                if len(self.stack) == 0:
                    raise StopIteration
                self.prev = edge
            elif self.prev == 0:
                ans = self.stack[-1][0].value
                self.stack.append((self.stack[-1][0].right(), 1))
                self.prev = -1
                return ans
            else:
                self.stack.append((self.stack[-1][0].left(), 0))
        return ans
           
            
class IndexList:
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
   
    def _insert(self, node, i, value):
        if not (0 <= i <= node.size):
            raise IndexError('index out of bound')

        if node.level == 0:
            return Node(value, Node.sentinel, Node.sentinel, 1)

        k = 0
        if node.left() != None:
            k = node.left().size
        
        if i <= k:
            node.set_left(self._insert(node.left(), i, value))
        else:
            node.set_right(self._insert(node.right(), i - (k + 1), value))

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

    def _remove(self, node, i):
        if not (0 <= i < node.size):
            raise IndexError('index out of bound')
        
        kid = -1
        
        k = 0
        if node.left() != None:
            k = node.left().size

        if i == k:
            if node.right().level == 0:
                return Node.sentinel
            else:
                t = self._succecor(node)
                self._swap(t, node)
                kid = 1
         
        if kid == -1:
            kid = 0 if i < k else 1

        if kid == 0:
            node.set_left(self._remove(node.left(), i))
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
            node.set_right(self._remove(node.right(), i - k - 1))
            if node.right().level < node.level - 1:
                assert node.right().level == node.level - 2
                node.level -= 1
                node = self._skew(node)
                node.set_right(self._skew(node.right()))
                node = self._split(node)
        
        return node 

    def insert(self, i, value):
        self._root = self._insert(self._root, i, value)
    
    def remove(self, i):
        self._root = self._remove(self._root, i)
    
    def _show(self, node, s):
        if node.level > 0:
            self._show(node.right(), s + 1)
            print(s * '\t', (node.value, node.level))
            self._show(node.left(), s + 1)

    def show(self):
        self._show(self._root, 0)
    
    def get(self, i):
        node = self._root
        while node != Node.sentinel:
            k = 0
            if node.left() != None:
                k = node.left().size
            if i == k:
                return node.value
            elif i < k:
                node = node.left()
            else:
                node = node.right()
                i -= k + 1
        raise IndexError('index out of bound')
    
    def __len__(self):
        return self._root.size

    def __iter__(self):
        return TreeIter(self._root)
