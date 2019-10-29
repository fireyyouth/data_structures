#include <iostream>
#include <cassert>
#include <string>
#include <memory>
#include <vector>
#include <bitset>
#include <optional>
#include <sstream>

using BitMap = std::bitset<256>;

static const std::vector<BitMap> mask = []() {
    std::vector<BitMap> a(256, 0);
    for (size_t i = 1; i < a.size(); ++i) {
        a[i] = (a[i - 1] << 1) | BitMap(1);
    }
    return a;
}();


template <typename T>
struct trie {
    struct Node : public std::enable_shared_from_this<Node> {
        using NodePtr = std::shared_ptr<const Node>;
        using DataPtr = std::shared_ptr<const T>;
        static const BitMap lshift(size_t i) {
            return ((uint64_t)1) << i;
        }

        DataPtr data;
        BitMap bitmap;
        std::vector< NodePtr > elements;

        Node(DataPtr d, const BitMap & b, std::vector< NodePtr > && e)
            : data(d), bitmap(b), elements(std::move(e)) {}
        Node() : bitmap(0) {}

        inline size_t InnerIndex(size_t i) const {
            return (bitmap & mask[i]).count();
        }

        NodePtr get(size_t i) const {
            if (bitmap.test(i)) {
                return elements[InnerIndex(i)];
            } else {
                return nullptr;
            }
        }

        size_t size() const {
            return elements.size();
        }

        NodePtr setData(const T & d) const {
            if (data && d == *data) {
                return this->shared_from_this();
            } else {
                std::vector< NodePtr > e(elements);
                return std::make_shared<Node>(std::make_shared<T>(d), bitmap, std::move(e));
            }
        }

        NodePtr setKid(size_t i, NodePtr kid) const {
            if (bitmap.test(i)) {
                if (kid != elements[InnerIndex(i)]) {
                    std::vector< NodePtr > e(elements);
                    e[InnerIndex(i)] = kid;
                    return std::make_shared<Node>(data, bitmap, std::move(e));
                } else {
                    return this->shared_from_this();
                }
            } else {
                size_t cnt = InnerIndex(i);
                std::vector< NodePtr > e(elements.size() + 1);
                std::copy(elements.begin(), elements.begin() + cnt, e.begin());
                e[cnt] = kid;
                std::copy(elements.begin() + cnt, elements.end(), e.begin() + cnt + 1);
                auto b(bitmap);
                b.set(i);
                return std::make_shared<Node>(data, b, std::move(e));
            }
        }

        NodePtr clearKid(size_t i) const {
            if (bitmap.test(i)) {
                size_t index = InnerIndex(i);
                std::vector< NodePtr > e(elements.size() - 1);
                std::copy(elements.begin(), elements.begin() + index, e.begin());
                std::copy(elements.begin() + index + 1, elements.end(), e.begin() + index);
                auto b(bitmap);
                b.reset(i);
                return std::make_shared<const Node>(data, b, std::move(e));
            } else {
                return this->shared_from_this();
            }
        }

        NodePtr clearData() const {
            if (data) {
                std::vector< NodePtr > e(elements);
                return std::make_shared<const Node>(nullptr, bitmap, std::move(e));
            } else {
                return this->shared_from_this();
            }
        }
    };

    using NodePtr = typename Node::NodePtr;
    using DataPtr = typename Node::DataPtr;

    static NodePtr remove(NodePtr head, const std::string & key) {
        return remove(head, reinterpret_cast<const uint8_t *>(key.data()), key.size());
    }

    static NodePtr remove(NodePtr head, const uint8_t *key, size_t len) {
        if (!head) {
            return head;
        } else {
            if (len == 0) {
                if (head->size()) {
                    return head->clearData();
                } else {
                    return nullptr;
                }
            } else {
                if (head->get(key[0])) {
                    auto p = remove(head->get(key[0]), key + 1, len - 1);
                    if (p) {
                        return head->setKid(key[0], p);
                    } else {
                        if (head->size() == 1 && !(head->data)) {
                            return nullptr;
                        } else {
                            return head->clearKid(key[0]);
                        }
                    }
                } else {
                    return head;
                }
            }
        }
    }

    static NodePtr insert(NodePtr head, const std::string & key, const T & data) {
        return insert(head, reinterpret_cast<const uint8_t *>(key.data()), key.size(), data);
    }

    static NodePtr insert(NodePtr head, const uint8_t *key, size_t len, const T & data) {
        if (!head) {
            std::vector< NodePtr > branches;
            auto p = std::make_shared<const Node>(std::make_shared<T>(data), BitMap(), std::move(branches));
            for (auto i = static_cast<long>(len) - 1; i >= 0; --i) {
                BitMap b;
                b.set(key[i]);
                std::vector< NodePtr > branches = {p,};
                p = std::make_shared<const Node>(nullptr, b, std::move(branches));
            }
            return p;
        } else {
            if (len == 0) {
                return head->setData(data);
            } else {
                auto p = insert(head->get(key[0]), key + 1, len - 1, data);
                return head->setKid(key[0], p);
            }
        }
    }

    static std::optional<T> find(NodePtr head, const std::string & key) {
        return find(head, reinterpret_cast<const uint8_t *>(key.data()), key.size());
    }

    static std::optional<T> find(NodePtr head, const uint8_t *key, size_t len) {
        auto p = head;
        for (size_t i = 0; i < len; ++i) {
            if (!p) {
                return std::nullopt;
            }
            p = p->get(key[i]);
        }
        if (p && p->data) {
            return *(p->data);
        } else {
            return std::nullopt;
        }
    }

    static std::vector<T> findPrefix(NodePtr head, const std::string & key) {
        return findPrefix(head, reinterpret_cast<const uint8_t *>(key.data()), key.size());
    }

    static std::vector<T> findPrefix(NodePtr head, const uint8_t *key, size_t len) {
        auto p = head;
        std::vector<T> r;
        for (size_t i = 0; i < len; ++i) {
            if (!p) {
                break;
            }
            if (p->data) {
                r.push_back(*(p->data));
            }
            p = p->get(key[i]);
        }
        if (p && p->data) {
            r.push_back(*(p->data));
        }
        return r;
    }

    static void dump(NodePtr head) {
        std::cout << "digraph G {\n";
        dump_node(head);
        std::cout << "}\n";
    }

    static std::string dump_node(NodePtr head) {
        if (!head) {
            return "empty";
        }
        std::stringstream ss;
        ss << "node_" << head.get();
        if (head->data) {
            ss << "_value_" << *(head->data);
        }
        const auto & head_name = ss.str();

        for (int i = 0; i < 256; ++i) {
            const auto & kid = head->get(i);
            if (kid) {
                const auto & kid_name = dump_node(kid);
                std::cout << head_name << " -> " << kid_name << " [ label=\"" << (char)i << "\" ];\n";
            }
        }
        if (head->data) {
            std::cout << head_name << " [style=filled, fillcolor=red];\n";
        }
        return head_name;
    }
};

void test_remove() {
    using IntTrie = trie<int>;
    IntTrie::NodePtr p;

    const size_t limit = 10000;
    for (int i = 0; i < limit; ++i) {
        if (i % 2 == 0) {
            p = IntTrie::insert(p, std::to_string(i), i);
        }
    }

    for (int i = 0; i < limit; ++i) {
        if (i % 3 != 0) {
            p = IntTrie::remove(p, std::to_string(i));
        }
    }

    for (int i = 0; i < limit; ++i) {
        const auto r = IntTrie::find(p, std::to_string(i));
        if (i % 2 == 0 && i % 3 == 0) {
            if ( !r ) {
                std::cerr << __LINE__ << ':' << i << std::endl;
            } else if (*r != i) {
                std::cerr << __LINE__ << ':' << i << std::endl;
            }
            assert ( r );
            assert (*r == i);
        } else {
            if ( r ) {
                std::cerr << __LINE__ << ':' << i << std::endl;
            }
            assert ( !r );
        }
    }
    // IntTrie::dump(p);
}

template<typename T>
bool vectorEqual(const std::vector<T> & a,const std::vector<T> & b) {
    if (a.size() != b.size()) {
        return false;
    } 
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}


void test_prefix() {
    using IntTrie = trie<int>;
    IntTrie::NodePtr p;
    p = IntTrie::insert(p, "123", 1);
    p = IntTrie::insert(p, "12345", 2);
    {
        auto r = IntTrie::findPrefix(p, "123");
        assert ( vectorEqual(r, std::vector<int>{1}) );
    }
    {
        auto r = IntTrie::findPrefix(p, "1234");
        assert ( vectorEqual(r, std::vector<int>{1}) );
    }
    {
        auto r = IntTrie::findPrefix(p, "12345");
        assert ( vectorEqual(r, std::vector<int>{1, 2}) );
    }
    {
        auto r = IntTrie::findPrefix(p, "123456");
        assert ( vectorEqual(r, std::vector<int>{1, 2}) );
    }
}

int main() {
    test_prefix();
    test_remove();
}
