#include <memory>
#include <variant>
#include <optional>
#include <vector>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cassert>

template <typename K, typename Value, typename KeyExtractor, typename Hasher, typename Comp = std::equal_to<K>>
class HAMT {
public:
    using HAMTPtr = std::shared_ptr<const HAMT>;
    using ValuePtr = std::shared_ptr<const Value>;
private:
    struct Node;
    using NodePtr = std::shared_ptr<const Node>;
    enum {
        INDEX_LEAF = 0,
        INDEX_NODE = 1
    };
    using VariantPtr = std::variant<ValuePtr, NodePtr>; // order is important

    static const size_t PERIOD = sizeof(size_t) * 8 / 6;

    static inline size_t gitBits(size_t hashcode,  size_t level) {
        return (hashcode >> (6 * (level % PERIOD))) & 63;
    }

    static inline uint64_t lshift(size_t i) {
        return ((uint64_t)1) << i;
    }

    struct Node : public std::enable_shared_from_this<Node> {
        uint64_t bitmap;
        std::vector< VariantPtr > elements;

        Node(uint64_t b, std::vector< VariantPtr > && e)
            : bitmap(b), elements(std::move(e)) {}
        Node() : bitmap(0) {}

        inline size_t InnerIndex(size_t i) const {
            return __builtin_popcountll(bitmap & (lshift(i) - 1));
        }

        std::optional< VariantPtr > get(size_t i) const {
            assert( i < 64 );
            if (lshift(i) & bitmap) {
                return elements[InnerIndex(i)];
            } else {
                return std::nullopt;
            }
        }

        size_t size() const {
            return elements.size();
        }

        NodePtr set(size_t i, VariantPtr kid) const {
            assert( i < 64 );
            if (lshift(i) & bitmap) {
                if (kid != elements[InnerIndex(i)]) {
                    std::vector< VariantPtr > e(elements);
                    e[InnerIndex(i)] = kid;
                    return std::make_shared<Node>(bitmap, std::move(e));
                } else {
                    return this->shared_from_this();
                }
            } else {
                size_t cnt = InnerIndex(i);
                std::vector< VariantPtr > e(elements.size() + 1);
                std::copy(elements.begin(), elements.begin() + cnt, e.begin());
                e[cnt] = kid;
                std::copy(elements.begin() + cnt, elements.end(), e.begin() + cnt + 1);
                return std::make_shared<Node>(bitmap | lshift(i), std::move(e));
            }
        }

        NodePtr clear(size_t i) const {
            assert( i < 64 );
            if (lshift(i) & bitmap) {
                size_t index = InnerIndex(i);
                std::vector< VariantPtr > e(elements.size() - 1);
                std::copy(elements.begin(), elements.begin() + index, e.begin());
                std::copy(elements.begin() + index + 1, elements.end(), e.begin() + index);
                return std::make_shared<const Node>(bitmap & ~(lshift(i)), std::move(e));
            } else {
                return this->shared_from_this();
            }
        }
    };


    NodePtr root_;
    size_t size_;

public:
    HAMT() : root_(std::make_shared<Node>()), size_(0) {}
    HAMT(const NodePtr & r, size_t s) : root_(r), size_(s) {}

    static HAMTPtr create() {
        return std::make_shared<HAMT>();
    }

    static size_t size(const HAMTPtr & hamt) {
        return hamt->size_;
    }

    static ValuePtr find(const HAMTPtr & hamt, const K & key) {
        auto p = hamt->root_;
        size_t hashcode = Hasher()(key, 0);
        size_t level = 0;
        while (1) {
            size_t bits = gitBits(hashcode, level);
            auto vp = p->get(bits);
            if (vp) {
                if (vp->index() == INDEX_LEAF) {
                    if (Comp()(key, KeyExtractor()(*std::get<INDEX_LEAF>(*vp)))) {
                        return std::get<INDEX_LEAF>(*vp);
                    } else {
                        return nullptr;
                    }
                } else {
                    p = std::get<INDEX_NODE>(*vp);
                }
            } else {
                return nullptr;
            }
            ++level;
            if (level % PERIOD == 0) {
                hashcode = Hasher()(key, level / PERIOD);
            }
        }
    }

    static HAMTPtr remove(const HAMTPtr & hamt, const K & key) {
        struct Frame {
            NodePtr node;
            size_t bits;
            Frame(NodePtr n, size_t b) : node(n), bits(b) {}
        };
        std::vector<Frame> stack;
        bool removed = false;

        {
            auto p = hamt->root_;
            size_t hashcode = Hasher()(key, 0);
            size_t level = 0;
            while (1) {
                size_t bits = gitBits(hashcode, level);
                stack.emplace_back(p, bits);
                auto vp = p->get(bits);
                if (vp) {
                    if (vp->index() == INDEX_LEAF) {
                        if (Comp()(key, KeyExtractor()(*std::get<INDEX_LEAF>(*vp)))) {
                            removed = true;
                            break;
                        }
                    } else {
                        p = std::get<INDEX_NODE>(*vp);
                    }
                } else {
                    break;
                }
                ++level;
                if (level % PERIOD == 0) {
                    hashcode = Hasher()(key, level / PERIOD);
                }
            }
        }

        if (removed) {
            NodePtr p;
            const auto & lastNode = stack.back().node;
            const auto & lastBits = stack.back().bits;
            VariantPtr t;
            if (stack.size() > 1 && lastNode->elements.size() == 2 && (t = lastNode->elements[1 - lastNode->InnerIndex(lastBits)]).index() == INDEX_LEAF) {
                stack.pop_back();
                while (stack.size() > 1 && stack.back().node->elements.size() == 1) {
                    stack.pop_back();
                }
                p = stack.back().node->set(stack.back().bits, t);
                stack.pop_back();
            } else {
                p = lastNode->clear(lastBits);
                stack.pop_back();
            }
            while (!stack.empty()) {
                p = stack.back().node->set(stack.back().bits, p);
                stack.pop_back();
            }
            return std::make_shared<HAMT>(p, hamt->size_ - 1);
        } else {
            return hamt;
        }
    }


    static NodePtr merge(const ValuePtr & a, size_t hash_a, const ValuePtr & b, size_t hash_b, size_t level) {
        size_t bits_a = gitBits(hash_a, level);
        size_t bits_b = gitBits(hash_b, level);
        if (bits_a == bits_b) {
            if ((level + 1) % PERIOD == 0) {
                hash_a = Hasher()(KeyExtractor()(*a), (level + 1) / PERIOD);
                hash_b = Hasher()(KeyExtractor()(*b), (level + 1) / PERIOD);
            }
            auto p = merge(a, hash_a, b, hash_b, level + 1);
            std::vector< VariantPtr > elements = {p,};
            return std::make_shared<Node>(lshift(bits_a), std::move(elements));
        } else {
            uint64_t bitmap = lshift(bits_a) | lshift(bits_b);
            if (bits_a < bits_b) {
                return std::make_shared<Node>(bitmap, std::vector<VariantPtr>{a, b});
            } else {
                return std::make_shared<Node>(bitmap, std::vector<VariantPtr>{b, a});
            }
        }
    }

    static NodePtr insert(const NodePtr & root, const ValuePtr & leaf, size_t hashcode, size_t level, bool & replaced) {
        size_t bits = gitBits(hashcode, level);
        auto vp = root->get(bits);

        if (!vp) {
            return root->set(bits, leaf);
        } else {
            if (vp->index() == INDEX_NODE) {
                if ((level + 1) % PERIOD == 0) {
                    hashcode = Hasher()(KeyExtractor()(*leaf), (level + 1) / PERIOD);
                }
                auto p = insert(std::get<INDEX_NODE>(*vp), leaf, hashcode, level + 1, replaced);
                return root->set(bits, p);
            } else {
                auto old_leaf = std::get<INDEX_LEAF>(*vp);
                if (Comp()(KeyExtractor()(*leaf), KeyExtractor()(*old_leaf))) {
                    replaced = true;
                    return root->set(bits, leaf);
                } else {
                    size_t old_leaf_hash = Hasher()(KeyExtractor()(*old_leaf), (level + 1) / PERIOD);
                    if ((level + 1) % PERIOD == 0) {
                        hashcode = Hasher()(KeyExtractor()(*leaf), (level + 1) / PERIOD);
                    }
                    auto p = merge(old_leaf, old_leaf_hash, leaf, hashcode, level + 1);
                    return root->set(bits, p);
                }
            }
        }
    }

    static std::pair<HAMTPtr, ValuePtr> insert(const HAMTPtr & hamt, const Value & value) {
        auto leaf = std::make_shared<Value>(value);
        bool replaced = false;
        const auto & root = insert(hamt->root_, leaf, Hasher()(KeyExtractor()(value), 0), 0, replaced);
        size_t size = hamt->size_;
        if (!replaced) {
            ++size;
        }
        return std::make_pair(std::make_shared<HAMT>(root, size), leaf);
    }

    /*
    static void toDot(NodePtr root, std::ostream & os) {
        os << "digraph {\n"
          "graph [pad=\"0.5\", nodesep=\"0.5\", ranksep=\"2\"];\n"
          "node [shape=plain]\n"
          "rankdir=LR;\n\n";

        _toDot(root, os);
        os << "}\n";
    }

    static inline std::string addrToName(const void *p) {
        std::stringstream ss;
        ss << "node_" << p;
        return ss.str();
    }

    static std::string _toDot(VariantPtr root, std::ostream & os) {
        if (root.index() == INDEX_LEAF) {
            std::stringstream ss;
            const auto & leaf = *(std::get<INDEX_LEAF>(root));
            ss << "leaf_" << KeyExtractor()(leaf);
            return ss.str();
        } else {
            auto p = std::get<INDEX_NODE>(root);
            std::string parent_name = addrToName(p.get());
            os << parent_name << " [label=<\n"
                    "  <table border=\"0\" cellborder=\"1\" cellspacing=\"0\">\n"
                    "    <tr><td><b><i>" << parent_name << "</i></b></td></tr>\n";
            
            for (size_t i = 0; i < 64; ++i) {
                auto e = p->get(i);
                if (e) {
                    os << "    <tr><td port=\"" << i << "\">" << i << "</td></tr>\n";
                }
            }
            os << "  </table>>];\n";

            for (size_t i = 0; i < 64; ++i) {
                auto e = p->get(i);
                if (e) {
                    const auto & kid_name = _toDot(*e, os); 
                    os << "    " << parent_name << ":" << i << " -> " << kid_name << "\n";
                }
            }
            return parent_name;
        }
    }
    */
};

template <typename K, typename V, typename Hasher, typename Comp = std::equal_to<K>>
struct HAMTMap {
    using Pair = std::pair<K, V>;
    struct GetFirst {
        K operator()(const Pair & p) {
            return p.first;
        }
    };
    using Impl = HAMT<K, Pair, GetFirst, Hasher, Comp>;
    using HAMTMapPtr = typename Impl::HAMTPtr;
    using ValuePtr = typename Impl::ValuePtr;

    static std::optional<V> find(const HAMTMapPtr & p, const K & key) {
        const auto & r = Impl::find(p, key);
        if (!r) {
            return std::nullopt;
        } else {
            return r->second;
        }
    }

    static size_t size(const HAMTMapPtr & p) {
        return Impl::size(p);
    }

    static std::pair<HAMTMapPtr, ValuePtr> insert(const HAMTMapPtr & p, const K & key, const V & value) {
        return Impl::insert(p, std::make_pair(key, value));
    }

    static HAMTMapPtr remove(const HAMTMapPtr & p, const K & key) {
        return Impl::remove(p, key);
    }
    /*
    static void toDot(HAMTMapPtr root, std::ostream & os) {
        Impl::toDot(root, os);
    }*/
    static HAMTMapPtr create() {
        return Impl::create();
    }
};


template <typename V, typename Hasher, typename Comp = std::equal_to<V>>
struct HAMTSet {
    struct Identity {
        V operator()(const V & v) {
            return v;
        }
    };
    using Impl = HAMT<V, V, Identity, Hasher, Comp>;
    using HAMTSetPtr = typename Impl::HAMTPtr;
    using ValuePtr = typename Impl::ValuePtr;
    static std::optional<V> find(const HAMTSetPtr & p, const V & key) {
        const auto & r = Impl::find(p, key);
        if (r) {
            return *r;
        } else {
            return std::nullopt;
        }
    }

    static std::pair<HAMTSetPtr, ValuePtr> insert(const HAMTSetPtr & root, const V & key) {
        return Impl::insert(root, key);
    }

    static HAMTSetPtr remove(const HAMTSetPtr & root, const V & key) {
        return Impl::remove(root, key);
    }
    /*
    static void toDot(HAMTSetPtr root, std::ostream & os) {
        Impl::toDot(root, os);
    }*/
    static HAMTSetPtr create() {
        return Impl::create();
    }

    static size_t size(const HAMTSetPtr & p) {
        return Impl::size(p);
    }
};


#include <string>


#include <map>
#include <cassert>

/*
void print_hash_bits(const std::string & s) {
    size_t h = std::hash<std::string>()(s);
    for (size_t i = 0; i < PERIOD; ++i) {
        std::cout << gitBits(h, i) << ' ';
    }
    std::cout << std::endl;
}
*/

void test_rehash() {
    struct BadStringHasher {
        size_t operator()(const std::string & s, size_t n) {
            size_t sum = 0;
            if (s.size() > 0) {
                sum = s[0] * n;
                for (char ch : s) {
                    sum += ch;
                }
            }
            return sum;
        }
    };

    using StringMap = HAMTMap<std::string, int, BadStringHasher>;

    auto p = StringMap::create();

    {
        const auto & r = StringMap::insert(p, "123", 1);
        p = r.first;
    }

    assert(StringMap::size(p) ==  1);

    {
        const auto & r = StringMap::insert(p, "321", 2);
        p = r.first;
    }

    assert(StringMap::size(p) ==  2);

    {
        const auto & r = StringMap::insert(p, "321", 2);
        p = r.first;
    }

    assert(StringMap::size(p) ==  2);

    {
        const auto & r = StringMap::insert(p, "321", 3);
        p = r.first;
    }

    assert(StringMap::size(p) ==  2);

    {
        auto r = StringMap::find(p, "123");
        assert (r);
        assert (*r == 1);
    }
    {
        auto r = StringMap::find(p, "321");
        assert (r);
        assert (*r == 3);
    }

    p = StringMap::remove(p, "321");

    assert(StringMap::size(p) ==  1);

    p = StringMap::remove(p, "321");

    assert(StringMap::size(p) ==  1);

    p = StringMap::remove(p, "123");

    assert(StringMap::size(p) ==  0);

}

void test_remove() {
    struct GoodStringHasher {
        size_t operator()(const std::string & s, size_t n) {
            size_t hash = 7 + n;
            for (char ch : s) {
                hash = hash * (31 + n) + ch;
            }
            return hash;
        }
    };

    using StringMap = HAMTMap<std::string, int, GoodStringHasher>;

    auto p = StringMap::create();

    const size_t limit = 1024;
    for (int i = 0; i < limit; ++i) {
        if (i % 2 == 0) {
            const auto & r = StringMap::insert(p, std::to_string(i), i);
            p = r.first;
        }
    }
    for (int i = 0; i < limit; ++i) {
        if (i % 3 != 0) {
            p = StringMap::remove(p, std::to_string(i));
        }
    }

    // StringMap::toDot(p, std::cout);

    for (int i = 0; i < limit; ++i) {
        const auto r = StringMap::find(p, std::to_string(i));
        if (i % 2 == 0 && i % 3 == 0) {
            assert ( r );
            assert (*r == i);
        } else {
            assert ( !r );
        }
    }
}

void test_set() {
    struct GoodStringHasher {
        size_t operator()(const std::string & s, size_t n) {
            size_t hash = 7 + n;
            for (char ch : s) {
                hash = hash * (31 + n) + ch;
            }
            return hash;
        }
    };

    using StringSet = HAMTSet<std::string, GoodStringHasher>;

    auto p = StringSet::create();

    assert (StringSet::size(p) == 0);
    const size_t limit = 1024;
    for (int i = 0; i < limit; ++i) {
        if (i % 2 == 0) {
            const auto & r = StringSet::insert(p, std::to_string(i));
            p = r.first;
        }
    }
    for (int i = 0; i < limit; ++i) {
        if (i % 3 != 0) {
            p = StringSet::remove(p, std::to_string(i));
        }
    }

    // StringSet::toDot(p, std::cout);

    for (int i = 0; i < limit; ++i) {
        const auto r = StringSet::find(p, std::to_string(i));
        if (i % 2 == 0 && i % 3 == 0) {
            assert ( r );
            assert (*r == std::to_string(i));
        } else {
            assert ( !r );
        }
    }
}
int main() {
    test_rehash();
    test_remove();
    test_set();
}
