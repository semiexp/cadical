#ifndef _ext_subgraph_connectivity_hpp_INCLUDED
#define _ext_subgraph_connectivity_hpp_INCLUDED

#include "../src/cadical.hpp"
#include "../src/extra_constraint.hpp"

namespace CaDiCaL {

class SubgraphConnectivity : public ExtraConstraint {
public:
    SubgraphConnectivity(const std::vector<int>& elits, const std::vector<std::pair<int, int>>& edges);
    virtual ~SubgraphConnectivity() = default;

    bool initialize(Internal& solver) override;
    bool propagate(Internal& solver, int p) override;
    std::vector<int> calc_reason(Internal& solver, int p) override;
    void undo(Internal& solver, int p) override;

private:
    enum NodeState {
        kUndecided, kActive, kInactive
    };
    const int kUnvisited = -1;

    int buildTree(int v, int parent, int cluster_id);

    std::vector<int> elits_, lits_;
    std::vector<std::pair<int, int>> var_to_idx_;
    std::vector<std::vector<int>> adj_;
    std::vector<NodeState> state_;
    std::vector<int> decision_order_;
    std::vector<int> rank_, lowlink_, subtree_active_count_, cluster_id_, parent_;
    int next_rank_;
    int conflict_cause_lit_;  // The conflict detected in propagate() is caused because `conflict_cause_pos_`-th variable was actually `conflict_cause_lit_`
    int conflict_cause_pos_;
    int n_active_vertices_;
};

};

#endif
