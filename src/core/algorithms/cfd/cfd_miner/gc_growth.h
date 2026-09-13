#pragma once

#include <cstddef>
#include <map>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>

#include "core/algorithms/cfd/model/cfd_types.h"

namespace algos::cfd {

class GCGrowth {
public:
    struct Generator {
        Itemset items;
        std::size_t support;
    };

    using ClosedToFree = std::map<Itemset, std::vector<Generator>>;

    ClosedToFree Mine(std::vector<Transaction> const& transactions, std::size_t min_support,
             std::size_t max_lhs);

private:
    struct Node {
        Item item;
        std::size_t parent;
        std::size_t support = 0;
        std::unordered_map<Item, std::size_t> children;
    };

    struct PatternState {
        std::size_t support;
        bool is_generator;
    };

    using PatternIndex = std::unordered_map<Itemset, PatternState, boost::hash<Itemset>>;
    using ClosedToFreeIndex = std::unordered_map<Itemset, std::vector<Generator>, boost::hash<Itemset>>;

    std::vector<Node> tree_;
    std::unordered_map<Item, std::vector<std::size_t>> headers_;
    std::unordered_map<Item, std::size_t> ranks_;
    std::vector<Item> ordered_items_;
    PatternIndex patterns_;
    ClosedToFreeIndex closed_to_free_;
    std::size_t min_support_;
    std::size_t max_lhs_;

    void BuildTree(std::vector<Transaction> const& transactions);
    bool ContainsPattern(std::size_t node, Itemset const& pattern) const;
    std::vector<std::size_t> FindMatchingPrefixes(Itemset const& pattern) const;
    Itemset ComputeClosure(std::vector<std::size_t> const& prefixes, std::size_t support) const;
    bool IsGenerator(Itemset const& pattern, std::size_t support) const;
    void Grow(Itemset const& prefix, std::size_t extension_end);
};

}  // namespace algos::cfd
