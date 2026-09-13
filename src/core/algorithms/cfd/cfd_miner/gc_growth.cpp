#include "gc_growth.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace algos::cfd {

GCGrowth::ClosedToFree GCGrowth::Mine(std::vector<Transaction> const& transactions,
                                               std::size_t min_support, std::size_t max_lhs) {
    if (min_support == 0) {
        throw std::invalid_argument("Minimum support must be greater than zero.");
    }

    tree_.clear();
    headers_.clear();
    ranks_.clear();
    ordered_items_.clear();
    patterns_.clear();
    closed_to_free_.clear();
    min_support_ = min_support;
    max_lhs_ = max_lhs;

    if (transactions.size() < min_support_) {
        return {};
    }

    BuildTree(transactions);
    patterns_.emplace(Itemset{}, PatternState{transactions.size(), true});
    closed_to_free_[ComputeClosure({0}, transactions.size())].push_back({{}, transactions.size()});
    Grow({}, ordered_items_.size());

    ClosedToFree result;
    for (auto& [closure, generators] : closed_to_free_) {
        result.emplace(std::move(closure), std::move(generators));
    }
    return result;
}

void GCGrowth::BuildTree(std::vector<Transaction> const& transactions) {
    std::unordered_map<Item, std::size_t> frequencies;
    for (auto row : transactions) {
        std::sort(row.begin(), row.end());
        row.erase(std::unique(row.begin(), row.end()), row.end());
        for (Item item : row) {
            ++frequencies[item];
        }
    }

    for (auto const& [item, support] : frequencies) {
        if (support >= min_support_) {
            ordered_items_.push_back(item);
        }
    }
    std::sort(ordered_items_.begin(), ordered_items_.end(), [&](Item left, Item right) {
        if (frequencies.at(left) != frequencies.at(right)) {
            return frequencies.at(left) > frequencies.at(right);
        }
        return left < right;
    });
    for (std::size_t rank = 0; rank < ordered_items_.size(); ++rank) {
        ranks_.emplace(ordered_items_[rank], rank);
    }

    tree_.push_back(Node{0, 0, transactions.size(), {}});
    for (auto row : transactions) {
        row.erase(std::remove_if(row.begin(), row.end(),
                                 [&](Item item) { return !ranks_.contains(item); }),
                  row.end());
        std::sort(row.begin(), row.end(),
                  [&](Item left, Item right) { return ranks_.at(left) < ranks_.at(right); });
        row.erase(std::unique(row.begin(), row.end()), row.end());

        std::size_t current = 0;
        for (Item item : row) {
            auto child = tree_[current].children.find(item);
            std::size_t next;
            if (child == tree_[current].children.end()) {
                next = tree_.size();
                tree_[current].children.emplace(item, next);
                tree_.push_back(Node{item, current, 0, {}});
                headers_[item].push_back(next);
            } else {
                next = child->second;
            }
            current = next;
            ++tree_[current].support;
        }
    }
}

bool GCGrowth::ContainsPattern(std::size_t node, Itemset const& pattern) const {
    auto required = pattern.rbegin();
    while (node != 0 && required != pattern.rend()) {
        if (tree_[node].item == *required) {
            ++required;
        }
        node = tree_[node].parent;
    }
    return required == pattern.rend();
}

std::vector<std::size_t> GCGrowth::FindMatchingPrefixes(Itemset const& pattern) const {
    std::vector<std::size_t> matches;
    for (std::size_t node : headers_.at(pattern.back())) {
        if (ContainsPattern(node, pattern)) {
            matches.push_back(node);
        }
    }
    return matches;
}

Itemset GCGrowth::ComputeClosure(std::vector<std::size_t> const& prefixes,
                                          std::size_t support) const {
    std::unordered_map<Item, std::size_t> item_supports;
    std::vector<std::size_t> pending;

    for (std::size_t prefix : prefixes) {
        for (std::size_t ancestor = tree_[prefix].parent; ancestor != 0;
             ancestor = tree_[ancestor].parent) {
            item_supports[tree_[ancestor].item] += tree_[prefix].support;
        }
        pending.push_back(prefix);
    }

    while (!pending.empty()) {
        std::size_t node = pending.back();
        pending.pop_back();
        if (node != 0) {
            item_supports[tree_[node].item] += tree_[node].support;
        }
        for (auto const& [item, child] : tree_[node].children) {
            pending.push_back(child);
        }
    }

    Itemset closure;
    closure.reserve(item_supports.size());
    for (auto const& [item, item_support] : item_supports) {
        if (item_support == support) {
            closure.push_back(item);
        }
    }
    std::sort(closure.begin(), closure.end());
    return closure;
}

bool GCGrowth::IsGenerator(Itemset const& pattern, std::size_t support) const {
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        auto subset = pattern;
        subset.erase(subset.begin() + index);
        auto parent = patterns_.find(subset);
        if (parent == patterns_.end() || !parent->second.is_generator ||
            parent->second.support <= support) {
            return false;
        }
    }
    return true;
}

void GCGrowth::Grow(Itemset const& prefix, std::size_t extension_end) {
    if (prefix.size() >= max_lhs_) {
        return;
    }

    for (std::size_t index = 0; index < extension_end; ++index) {
        auto pattern = prefix;
        pattern.insert(pattern.begin(), ordered_items_[index]);
        auto matches = FindMatchingPrefixes(pattern);
        std::size_t support = 0;
        for (std::size_t node : matches) {
            support += tree_[node].support;
        }

        bool is_generator = support >= min_support_ && IsGenerator(pattern, support);
        patterns_.emplace(pattern, PatternState{support, is_generator});
        if (!is_generator) {
            continue;
        }

        Itemset output_generator = pattern;
        std::sort(output_generator.begin(), output_generator.end());
        closed_to_free_[ComputeClosure(matches, support)].push_back(
                {std::move(output_generator), support});
        Grow(pattern, index);
    }
}

}  // namespace algos::cfd
