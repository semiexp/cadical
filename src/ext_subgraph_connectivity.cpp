#include "ext_subgraph_connectivity.hpp"

#include <set>
#include <algorithm>

class UnionFind {
public:
    UnionFind(int n) : parent_(n, -1), n_active_(n, 0), n_active_clusters_(0) {}

    int Root(int p) const {
        while (parent_[p] >= 0) p = parent_[p];
        return p;
    }
    int NumActiveClusters() const {
        return n_active_clusters_;
    }
    void Merge(int p, int q) {
        p = Root(p);
        q = Root(q);
        if (p == q) return;
        if (parent_[p] > parent_[q]) std::swap(p, q);

        UpdateParent(p, parent_[p] + parent_[q]);
        UpdateParent(q, p);

        int nac_updated = n_active_clusters_;
        nac_updated -= (n_active_[p] > 0 ? 1 : 0);
        nac_updated -= (n_active_[q] > 0 ? 1 : 0);
        nac_updated += (n_active_[p] + n_active_[q] > 0 ? 1 : 0);
        UpdateNActive(p, n_active_[p] + n_active_[q]);
        UpdateNActive(q, 0);
        UpdateNActiveClusters(nac_updated);
    }
    void AddActiveCount(int p, int d) {
        int nac_updated = n_active_clusters_;
        p = Root(p);
        nac_updated -= (n_active_[p] > 0 ? 1 : 0);
        UpdateNActive(p, n_active_[p] + d);
        nac_updated += (n_active_[p] > 0 ? 1 : 0);
        UpdateNActiveClusters(nac_updated);
    }
    void Commit() { redo_.clear(); }
    void Redo() {
        while (!redo_.empty()) {
            int loc = redo_.back().first, val = redo_.back().second;
            redo_.pop_back();

            if (loc == -1) n_active_clusters_ = val;
            else if (loc % 2 == 0) parent_[loc / 2] = val;
            else n_active_[loc / 2] = val;
        }
    }

private:
    void UpdateParent(int p, int v) {
        if (parent_[p] == v) return;
        redo_.push_back({p * 2, parent_[p]});
        parent_[p] = v;
    }
    void UpdateNActive(int p, int v) {
        if (n_active_[p] == v) return;
        redo_.push_back({p * 2 + 1, n_active_[p]});
        n_active_[p] = v;
    }
    void UpdateNActiveClusters(int v) {
        if (n_active_clusters_ == v) return;
        redo_.push_back({-1, n_active_clusters_});
        n_active_clusters_ = v;
    }

    std::vector<int> parent_, n_active_;
    std::vector<std::pair<int, int>> redo_;
    int n_active_clusters_;
};

namespace CaDiCaL {

SubgraphConnectivity::SubgraphConnectivity(const std::vector<int>& elits, const std::vector<std::pair<int, int>>& edges)
    : elits_(elits), adj_(elits.size()), state_(elits.size(), kUndecided), 
      rank_(elits.size()), lowlink_(elits.size()), subtree_active_count_(elits.size()), cluster_id_(elits.size()),
      parent_(elits.size()), conflict_cause_pos_(-2), n_active_vertices_(0) {
    for (auto& e : edges) {
        adj_[e.first].push_back(e.second);
        adj_[e.second].push_back(e.first);
    }
}

bool SubgraphConnectivity::initialize(Internal& solver) {
    for (int elit : elits_) {
        lits_.push_back(solver.external->internalize(elit));
    }

    for (size_t i = 0; i < lits_.size(); ++i) {
        var_to_idx_.push_back({solver.vidx(lits_[i]), i});
    }
    sort(var_to_idx_.begin(), var_to_idx_.end());

    for (size_t i = 0; i < lits_.size(); ++i) {
        signed char val = solver.val(lits_[i]);
        if (val != 0) decision_order_.push_back(i);
        if (val == 1) state_[i] = kActive;
        else if (val == -1) state_[i] = kInactive;
    }
    std::set<int> lits_unique;
    for (int l : lits_) {
        lits_unique.insert(l);
        lits_unique.insert(-l);
    }
    for (int l : lits_unique) {
        solver.require_extra_watch(l, this);
    }

    std::set<int> propagate_lits;
    for (size_t i = 0; i < lits_.size(); ++i) {
        if (state_[i] == kActive) {
            propagate_lits.insert(lits_[i]);
        } else if (state_[i] == kInactive) {
            propagate_lits.insert(-lits_[i]);
        }
    }
    for (auto l : propagate_lits) {
        if (!propagate(solver, l)) return false;
    }
    return true;
}

bool SubgraphConnectivity::propagate(Internal& solver, int p) {
    // TODO: introduce lazy propagation

    int n = lits_.size();

    for (auto it = std::lower_bound(var_to_idx_.begin(), var_to_idx_.end(), std::make_pair(solver.vidx(p), -1)); it != var_to_idx_.end() && it->first == solver.vidx(p); ++it) {
        int i = it->second;
        signed char val = solver.val(lits_[i]);
        NodeState s;
        if (val == 1) {
            s = kActive;
            ++n_active_vertices_;
        } else if (val == -1) s = kInactive;
        else abort();
        // TODO: pass this check (this fails only when invoked from `initialize` and this does not actually cause problems, though)
        // assert(state_[i] == kUndecided);
        state_[i] = s;
        decision_order_.push_back(i);
    }

    if (n_active_vertices_ == 0) return true;

    std::fill(rank_.begin(), rank_.end(), kUnvisited);
    std::fill(lowlink_.begin(), lowlink_.end(), kUndecided);
    std::fill(subtree_active_count_.begin(), subtree_active_count_.end(), 0);
    std::fill(cluster_id_.begin(), cluster_id_.end(), kUndecided);
    std::fill(parent_.begin(), parent_.end(), -1);
    next_rank_ = 0;

    int nonempty_cluster = -1, n_all_clusters = 0;

    for (int i = 0; i < n; ++i) {
        if (state_[i] != kInactive && rank_[i] == kUnvisited) {
            buildTree(i, -1, i);
            int sz = subtree_active_count_[i];
            if (sz >= 1) {
                if (nonempty_cluster != -1) {
                    conflict_cause_pos_ = -1;
                    return false; // already disconnected
                }
                nonempty_cluster = i;
            } else {
                ++n_all_clusters;
            }
        }
    }

    if (n_active_vertices_ <= 1 && n_all_clusters == 0) return true;

    if (nonempty_cluster != -1) {
        for (int v = 0; v < n; ++v) {
            if (state_[v] != kUndecided) continue;

            if (cluster_id_[v] != nonempty_cluster) {
                // nodes outside the nonempty cluster should be inactive
                signed char val = solver.val(-lits_[v]);
                if (val == 1) {
                    // do nothing
                } else if (val == 0) {
                    solver.search_assign_ext(-lits_[v], this);
                } else {
                    conflict_cause_pos_ = v;
                    conflict_cause_lit_ = lits_[v];
                    return false;
                }
            } else {
                if (n_active_vertices_ <= 1) continue;
                // check if node `v` is an articulation point
                int parent_side_count = subtree_active_count_[nonempty_cluster] - subtree_active_count_[v];
                int n_nonempty_subgraph = 0;
                for (auto w : adj_[v]) {
                    if (rank_[v] < rank_[w] && parent_[w] == v) {
                        // `w` is a child of `v`
                        if (lowlink_[w] < rank_[v]) {
                            // `w` is not separated from `v`'s parent even after removal of `v`
                            parent_side_count += subtree_active_count_[w];
                        } else {
                            if (subtree_active_count_[w] > 0) ++n_nonempty_subgraph;
                        }
                    }
                }
                if (parent_side_count > 0) ++n_nonempty_subgraph;
                if (n_nonempty_subgraph >= 2) {
                    // `v` is an articulation point
                    signed char val = solver.val(lits_[v]);
                    if (val == 1) {
                        // do nothing
                    } else if (val == 0) {
                        solver.search_assign_ext(lits_[v], this);
                    } else {
                        conflict_cause_pos_ = v;
                        conflict_cause_lit_ = -lits_[v];
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

int SubgraphConnectivity::buildTree(int v, int parent, int cluster_id) {
    rank_[v] = next_rank_++;
    cluster_id_[v] = cluster_id;
    parent_[v] = parent;
    int lowlink = rank_[v];
    int subtree_active_count = (state_[v] == kActive ? 1 : 0);

    for (int w : adj_[v]) {
        if (w == parent || state_[w] == kInactive) continue;
        if (rank_[w] == -1) {
            // unvisited
            lowlink = std::min(lowlink, buildTree(w, v, cluster_id));
            subtree_active_count += subtree_active_count_[w];
        } else {
            lowlink = std::min(lowlink, rank_[w]);
        }
    }

    subtree_active_count_[v] = subtree_active_count;
    return lowlink_[v] = lowlink;
}

std::vector<int> SubgraphConnectivity::calc_reason(Internal& solver, int p) {
    if (p == 0 && conflict_cause_pos_ == -2) abort();
    if (p == 0 && conflict_cause_pos_ != -1) {
        decision_order_.push_back(conflict_cause_pos_);
        if (conflict_cause_lit_ == lits_[conflict_cause_pos_]) {
            state_[conflict_cause_pos_] = kActive;
        } else {
            state_[conflict_cause_pos_] = kInactive;
        }
    }
    int n = lits_.size();
    UnionFind union_find(n);
    std::vector<bool> activated(n, false);

    std::vector<int> out_reason;

    for (int i = 0; i < n; ++i) {
        if (state_[i] == kActive) union_find.AddActiveCount(i, 1);
        if (state_[i] != kInactive && (p == 0 || p != lits_[i])) activated[i] = true;
    }
    for (int v = 0; v < n; ++v) {
        for (int w : adj_[v]) {
            if (activated[v] && activated[w]) union_find.Merge(v, w);
        }
    }
    for (int i = 0; i < n; ++i) {
        if (lits_[i] == -p) {
            union_find.AddActiveCount(i, 1);
        }
    }
    union_find.Commit();
    if (union_find.NumActiveClusters() <= 1) {
        abort();
    }
    for (int i = decision_order_.size() - 1; i >= 0; --i) {
        int v = decision_order_[i];
        if (p != 0 && solver.vidx(p) == solver.vidx(lits_[v])) {
            abort();
        }

        if (state_[v] == kActive) union_find.AddActiveCount(v, -1);
        for (int w : adj_[v]) {
            if (activated[w]) union_find.Merge(v, w);
        }

        if (union_find.NumActiveClusters() >= 2) {
            union_find.Commit();
            activated[v] = true;
        } else {
            union_find.Redo();
            if (state_[v] == kActive) out_reason.push_back(lits_[v]);
            else if (state_[v] == kInactive) out_reason.push_back(-lits_[v]);
        }
    }
    if (p == 0 && conflict_cause_pos_ != -1) {
        decision_order_.pop_back();
        state_[conflict_cause_pos_] = kUndecided;
    }
    return out_reason;
}

void SubgraphConnectivity::undo(Internal& solver, int p) {
    for (auto it = std::lower_bound(var_to_idx_.begin(), var_to_idx_.end(), std::make_pair(solver.vidx(p), -1)); it != var_to_idx_.end() && it->first == solver.vidx(p); ++it) {
        int i = it->second;
        if (state_[i] == kActive) --n_active_vertices_;
        state_[i] = kUndecided;
        decision_order_.pop_back();
    }
}

}
