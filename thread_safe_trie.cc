#include <iostream>
#include <cassert>
#include <string>
#include <memory>
#include <vector>

struct trie {
    struct Node;
    using NodePtr = std::shared_ptr<const Node>;

    struct Node {
        std::vector< NodePtr > branches;
        bool flag;

        Node(std::vector< NodePtr > && b, bool f) : branches(std::move(b)), flag(f) {}
    };

    static NodePtr insert(NodePtr head, const std::string & key) {
        return insert(head, key.data(), key.size());
    }

    static NodePtr insert(NodePtr head, const char *key, size_t len) {
        if (!head) {
            std::vector< NodePtr > branches(256);
            auto p = std::make_shared<const Node>(std::move(branches), true);
            for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
                std::vector< NodePtr > branches(256);
                branches[key[i]] = p;
                p = std::make_shared<const Node>(std::move(branches), false);
            }
            return p;
        } else {
            if (len == 0) {
                if (head->flag) {
                    return head;
                } else {
                    std::vector< NodePtr > branches(head->branches);
                    return std::make_shared<const Node>(std::move(branches), true);
                }
            } else {
                std::vector< NodePtr > branches(head->branches);
                branches[key[0]] = insert(head->branches[key[0]], key + 1, len - 1);
                return std::make_shared<const Node>(std::move(branches), head->flag);
            }
        }
    }

    static bool find(NodePtr head, const std::string & key) {
        return find(head, key.data(), key.size());
    }

    static bool find(NodePtr head, const char *key, size_t len) {
        auto p = head;
        for (size_t i = 0; i < len; ++i) {
            if (!p) {
                return false;
            }
            p = p->branches[key[i]];
        }
        return p && p->flag;
    }

    static void dump(NodePtr head) {
        std::cout << "digraph G {\n";
        dump_node(head);
        std::cout << "}\n";
    }

private:
    static void dump_node(NodePtr head) {
        if (!head) {
            return;
        }
        for (int i = 0; i < 256; ++i) {
            const auto & kid = head->branches[i];
            if (kid) {
                dump_node(kid);
                std::cout << "node_" << head.get() << " -> " << "node_" << kid.get() << " [ label=\"" << (char)i << "\" ];\n";
            }
        }
        if (head->flag) {
            std::cout << "node_" << head.get() << " [style=filled, fillcolor=red];\n";
        }
    }
};

int main() {
    trie::NodePtr t;
    t = trie::insert(t, "123");
    t = trie::insert(t, "12");
    t = trie::insert(t, "111");
    t = trie::insert(t, "112");


    trie::dump(t);
}
