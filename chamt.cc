#include <memory>
#include <variant>
#include <optional>
#include <vector>
#include <cstring>
#include <iostream>
#include <sstream>

struct HAMTBase {
    static const size_t PERIOD;

    static inline size_t getBits(size_t hashcode,  size_t level) {
        return (hashcode >> (6 * (level % PERIOD))) & 63;
    }
};

const size_t HAMTBase::PERIOD = sizeof(size_t) * 8 / 6;

template <typename T, typename Comp, typename Hasher, typename Rehasher>
struct CHAMT : public HAMTBase {
    struct Node;
    using Leaf = T;
    using LeafPtr = std::shared_ptr<const Leaf>;
    using NodePtr = std::shared_ptr<const Node>;
    enum {
        INDEX_LEAF = 0,
        INDEX_NODE = 1
    };
    using VariantPtr = std::variant<LeafPtr, NodePtr>; // order is important

    struct Node {
        uint64_t bitmap;
        std::vector< VariantPtr > elements;

        Node(uint64_t b, std::vector< VariantPtr > && e)
            : bitmap(b), elements(std::move(e)) {}
        Node() : bitmap(0) {}

        static inline uint64_t lshift(size_t i) {
            return ((uint64_t)1) << i;
        }

        inline size_t InnerIndex(size_t i) const {
            return __builtin_popcountll(bitmap & (lshift(i) - 1));
        }

        std::optional< VariantPtr > get(size_t i) const {
            if (lshift(i) & bitmap) {
                return elements[InnerIndex(i)];
            } else {
                return std::nullopt;
            }
        }
    };


    static NodePtr create() {
        return std::make_shared<Node>();
    }

    static std::optional<LeafPtr> find(NodePtr p, const T & data) {
        size_t hashcode = Hasher()(data);
        size_t level = 0;
        while (1) {
            size_t bits = getBits(hashcode, level);
            auto vp = p->get(bits);
            if (vp) {
                if (vp->index() == INDEX_LEAF) {
                    if (Comp()(data, *(std::get<INDEX_LEAF>(*vp)))) {
                        return std::get<INDEX_LEAF>(*vp);
                    } else {
                        return std::nullopt;
                    }
                } else {
                    p = std::get<INDEX_NODE>(*vp);
                }
            } else {
                return std::nullopt;
            }
            ++level;
            if (level % PERIOD == 0) {
                hashcode = Rehasher()(data, level);
            }
        }
    }

    static NodePtr remove(NodePtr root, const T & data) {
        struct Frame {
            const Node *node;
            size_t bits;
            Frame(const Node *n, size_t b) : node(n), bits(b) {}
        };
        std::vector<Frame> stack;
        bool removed = false;

        // search
        {
            size_t hashcode = Hasher()(data);
            size_t level = 0;
            auto p = root;
            while (1) {
                size_t bits = getBits(hashcode, level);
                stack.emplace_back(p.get(), bits);
                auto vp = p->get(bits);
                if (vp) {
                    if (vp->index() == INDEX_LEAF) {
                        if (Comp()(data, *(std::get<INDEX_LEAF>(*vp)))) {
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
                    hashcode = Rehasher()(data, level);
                }
            }
        }

        if (!removed) {
            return root;
        } else {
            const auto & back = stack.back();
            size_t index = back.node->InnerIndex(back.bits);
            NodePtr p;
            if (stack.size() > 1 && back.node->elements.size() == 2 && back.node->elements[1 - index].index() == INDEX_LEAF) {
                auto t = back.node->elements[1 - index];
                stack.pop_back();
                while (stack.size() > 1 && stack.back().node->elements.size() == 1) {
                    stack.pop_back();
                }
                auto elements = stack.back().node->elements;
                elements[stack.back().node->InnerIndex(stack.back().bits)] = t;
                p = std::make_shared<const Node>(stack.back().node->bitmap, std::move(elements));
                stack.pop_back();
            } else {
                std::vector< VariantPtr > elements(back.node->elements.size() - 1);
                std::copy(back.node->elements.begin(), back.node->elements.begin() + index, elements.begin());
                std::copy(back.node->elements.begin() + index + 1, back.node->elements.end(), elements.begin() + index);
                p = std::make_shared<const Node>(back.node->bitmap & ~(Node::lshift(back.bits)), std::move(elements));
                stack.pop_back();
            }
            while (!stack.empty()) {
                auto elements = stack.back().node->elements;
                elements[stack.back().node->InnerIndex(stack.back().bits)] = p;
                p = std::make_shared<const Node>(stack.back().node->bitmap, std::move(elements));
                stack.pop_back();
            }
            return p;
        }

    }


    static NodePtr merge(LeafPtr a, size_t hash_a, LeafPtr b, size_t hash_b, size_t level) {
        size_t bits_a = getBits(hash_a, level);
        size_t bits_b = getBits(hash_b, level);
        if (bits_a == bits_b) {
            if ((level + 1) % PERIOD == 0) {
                hash_a = Rehasher()(*a, level + 1);
                hash_b = Rehasher()(*b, level + 1);
                if (hash_a == hash_b) {
                    throw std::runtime_error("your rehasher sucks!");
                }
            }
            auto p = merge(a, hash_a, b, hash_b, level + 1);
            std::vector< VariantPtr > elements = {p,};
            return std::make_shared<Node>(Node::lshift(bits_a), std::move(elements));
        } else {
            uint64_t bitmap = Node::lshift(bits_a) | Node::lshift(bits_b);
            if (bits_a < bits_b) {
                return std::make_shared<Node>(bitmap, std::vector<VariantPtr>{a, b});
            } else {
                return std::make_shared<Node>(bitmap, std::vector<VariantPtr>{b, a});
            }
        }
    }

    static NodePtr insert(NodePtr root, const T & data, size_t hashcode, size_t level) {
        size_t bits = getBits(hashcode, level);
        auto vp = root->get(bits);
        if (!vp) {
            size_t cnt = root->InnerIndex(bits);
            std::vector< VariantPtr > elements(root->elements.size() + 1);
            std::copy(root->elements.begin(), root->elements.begin() + cnt, elements.begin());
            elements[cnt] = std::make_shared<Leaf>(data);
            std::copy(root->elements.begin() + cnt, root->elements.end(), elements.begin() + cnt + 1);
            return std::make_shared<Node>(root->bitmap | Node::lshift(bits), std::move(elements));
        } else {
            if (vp->index() == INDEX_NODE) {
                if ((level + 1) % PERIOD == 0) {
                    hashcode = Rehasher()(data, level + 1);
                    // throw std::runtime_error("rehash not ready!");
                }
                auto p = insert(std::get<INDEX_NODE>(*vp), data, hashcode, level + 1);
                if (p != std::get<INDEX_NODE>(*vp)) {
                    std::vector< VariantPtr > elements(root->elements);
                    elements[root->InnerIndex(bits)] = p;
                    return std::make_shared<Node>(root->bitmap, std::move(elements));
                } else {
                    return root;
                }
            } else {
                auto leaf = std::get<INDEX_LEAF>(*vp);
                if (Comp()(data, *leaf)) {
                    return root;
                } else {
                    if ((level + 1) % PERIOD == 0) {
                        hashcode = Rehasher()(data, level + 1);
                        // throw std::runtime_error("rehash not ready!");
                    }
                    auto p = merge(leaf, Hasher()(*leaf), std::make_shared<Leaf>(data), hashcode, level + 1);
                    std::vector< VariantPtr > elements(root->elements);
                    elements[root->InnerIndex(bits)] = p;
                    return std::make_shared<Node>(root->bitmap, std::move(elements));
                }
            }
        }
    }

    static NodePtr insert(NodePtr root, const T & data) {
        return insert(root, data, Hasher()(data), 0);
    }

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
            ss << "value_" << *(std::get<INDEX_LEAF>(root));
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
};



template <typename K, typename V, typename Comp, typename Hasher, typename Rehasher>
struct ImmutableMap {
    using Pair = std::pair<K, V>;
    struct PairComp {
        bool operator()(const Pair & a, const Pair & b) {
            return Comp()(a.first, b.first);
        }
    };
    struct PairHasher {
        size_t operator()(const Pair & pair) {
            return Hasher()(pair.first);
        }
    };
    struct PairRehasher {
        size_t operator()(const Pair & pair, size_t n) {
            return Rehasher()(pair.first, n);
        }
    };

    using PairCHAMP = CHAMT<Pair, PairComp, PairHasher, PairRehasher>;
    using NodePtr = typename PairCHAMP::NodePtr;
    using LeafPtr = typename PairCHAMP::LeafPtr;

    static NodePtr create() {
        return PairCHAMP::create();
    }

    static std::optional<LeafPtr> find(NodePtr p, const K & key) {
        return PairCHAMP::find(p, Pair(key, V()));
    }

    static NodePtr insert(NodePtr p, const K & key, const V & value) {
        return PairCHAMP::insert(p, Pair(key, value));
    }

    static NodePtr remove(NodePtr p, const K & key) {
        return PairCHAMP::remove(p, Pair(key, V()));
    }
};


struct StringHasher {
    size_t operator()(const std::string & s) {
        size_t hash = 0;
        for (char ch : s) {
            hash = hash + ch;
        }
        return hash;
    }
};

struct StringRehasher {
    size_t operator()(const std::string & s, size_t n) {
        std::string t(s.size() + 2 * sizeof(n), '\0');
        memcpy(t.data(), &n, sizeof(n));
        memcpy(t.data() + sizeof(n), s.data(), s.size());
        memcpy(t.data() + sizeof(n) + s.size(), &n, sizeof(n));
        return std::hash<std::string>()(t);
    }
};


void test_immutable_map() {
    using StringIntMap = ImmutableMap<std::string, int, std::equal_to<std::string>, StringHasher, StringRehasher>;
    auto p = StringIntMap::create();

    for (int i = 0; i < 64; ++i) {
        if (i % 2) {
            p = StringIntMap::insert(p, std::to_string(i), i);
        }
    }

    for (int i = 0; i < 64; ++i) {
        auto result = StringIntMap::find(p, std::to_string(i));
        if (i % 2) {
            if (! result ) {
                std::cerr << __LINE__ << " : " <<  i << std::endl;
            } else {
                if ((*result)->second != i) {
                    std::cerr << __LINE__ << " : " << (*result)->second << " != " <<  i << std::endl;
                }
            }
        } else {
            if (result) {
                std::cerr << __LINE__ << " : " <<  i << std::endl;
            }
        }
    }

    for (int i = 0; i < 64; ++i) {
        if (i % 3 == 0) {
            p = StringIntMap::remove(p, std::to_string(i));
        }
    }

    for (int i = 0; i < 64; ++i) {
        auto result = StringIntMap::find(p, std::to_string(i));
        if( (i % 2 && i % 3) ) {
            if ( ! (result && (*result)->second == i) ) {
                std::cerr << i << std::endl;
            }
        } else {
            if (result) {
                std::cerr << i << std::endl;
            }
        }
    }
}


#include <string>


struct IntRehasher {
    size_t operator()(int s, size_t n) {
        return std::hash<int>()(s * n);
    }
};
/*
using IntSet = CHAMT<int, std::hash<int>, IntRehasher>;
using StringSet = CHAMT<std::string, StringHasher, StringRehasher>;
*/
#include <map>
#include <cassert>

template <typename Hasher>
void print_hash_bits(const std::string & s) {
    size_t h = Hasher()(s);
    for (size_t i = 0; i < HAMTBase::PERIOD; ++i) {
        std::cerr << HAMTBase::getBits(h, i) << ' ';
    }
    std::cerr << std::endl;
}

template <typename Rehasher>
void print_hash_bits(const std::string & s, size_t n) {
    size_t h = Rehasher()(s, n);
    for (size_t i = 0; i < HAMTBase::PERIOD; ++i) {
        std::cerr << HAMTBase::getBits(h, i) << ' ';
    }
    std::cerr << std::endl;
}

int main() {
    
    test_immutable_map();

    // StringSet::toDot(p, std::cout);

}