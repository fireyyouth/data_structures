#include <memory>
#include <variant>
#include <optional>
#include <vector>
#include <cstring>
#include <iostream>
#include <sstream>

    static const size_t PERIOD = sizeof(size_t) * 8 / 6;


    static inline size_t gitBits(size_t hashcode,  size_t level) {
        return (hashcode >> (6 * (level % PERIOD))) & 63;
    }

template <typename T, typename Rehasher, typename Hasher = std::hash<T>>
struct CHAMT {
    struct Node;
    using Leaf = T;
    using LeafPtr = std::shared_ptr<const Leaf>;
    using NodePtr = std::shared_ptr<const Node>;
    enum {
        INDEX_LEAF = 0,
        INDEX_NODE = 1
    };
    using VariantPtr = std::variant<LeafPtr, NodePtr>; // order is important


    static Hasher hasher;
    static Rehasher rehasher;

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

    static bool find(NodePtr p, const T & data) {
        size_t hashcode = Hasher()(data);
        size_t level = 0;
        while (1) {
            size_t bits = gitBits(hashcode, level);
            auto vp = p->get(bits);
            if (vp) {
                if (vp->index() == INDEX_LEAF) {
                    return data == *(std::get<INDEX_LEAF>(*vp));
                } else {
                    p = std::get<INDEX_NODE>(*vp);
                }
            } else {
                return false;
            }
            ++level;
            if (level % PERIOD == 0) {
                hashcode = Rehasher()(data, level);
            }
        }
    }


    static NodePtr merge(LeafPtr a, size_t hash_a, LeafPtr b, size_t hash_b, size_t level) {
        size_t bits_a = gitBits(hash_a, level);
        size_t bits_b = gitBits(hash_b, level);
        if (bits_a == bits_b) {
            if ((level + 1) % PERIOD == 0) {
                throw std::runtime_error("rehash not ready!");
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
        size_t bits = gitBits(hashcode, level);
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
                    throw std::runtime_error("rehash not ready!");
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
                if (data == *leaf) {
                    return root;
                } else {
                    if ((level + 1) % PERIOD == 0) {
                        throw std::runtime_error("rehash not ready!");
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

#include <string>

struct StringRehasher {
    size_t operator()(const std::string & s, size_t n) {
        std::string t(s.size() + 2 * sizeof(n), '\0');
        memcpy(t.data(), &n, sizeof(n));
        memcpy(t.data() + sizeof(n), s.data(), s.size());
        memcpy(t.data() + sizeof(n) + s.size(), &n, sizeof(n));
        return std::hash<std::string>()(t);
    }
};

struct IntRehasher {
    size_t operator()(int s, size_t n) {
        return std::hash<int>()(s * n);
    }
};

using IntSet = CHAMT<int, IntRehasher>;
using StringSet = CHAMT<std::string, StringRehasher>;

#include <map>
#include <cassert>

void print_hash_bits(const std::string & s) {
    size_t h = std::hash<std::string>()(s);
    for (size_t i = 0; i < PERIOD; ++i) {
        std::cout << gitBits(h, i) << ' ';
    }
    std::cout << std::endl;
}

int main() {
    auto p = StringSet::create();
    for (int i = 0; i < 128; ++i) {
        if (i % 2) {
            p = StringSet::insert(p, std::to_string(i));
        }
    }

    for (int i = 0; i < 128; ++i) {
        if( StringSet::find(p, std::to_string(i)) != (i % 2) ) {
            std::cout << i << std::endl;
        }
    }

    // std::cerr << StringSet::find(p, std::to_string(51));
    /*
    print_hash_bits(std::to_string(5));
    print_hash_bits(std::to_string(43));
    print_hash_bits(std::to_string(51));
    */
    // StringSet::toDot(p, std::cout);

}