#include "../../src/cadical.hpp"
#include "../../src/ext_subgraph_connectivity.hpp"
#include "common.hpp"

namespace {

int enumerate_connected_subgraph_by_sat(int n, const std::vector<std::pair<int, int>>& graph) {
  CaDiCaL::Solver solver;
  solver.set("chrono", 0);

  std::vector<int> vars;
  for (int i = 1; i <= n; ++i) {
    vars.push_back(i);
  }

  std::vector<int> lits;
  for (int i = 1; i <= n; ++i) {
    lits.push_back(i);
  }

  solver.add_extra(std::make_unique<CaDiCaL::SubgraphConnectivity>(lits, graph));
  return count_sat_assignment(solver, vars);
}

void visit(int p, int mask, const std::vector<std::vector<int>>& adj, std::vector<bool>& visited) {
  if (visited[p]) return;
  if (((mask >> p) & 1) == 0) return;

  visited[p] = true;
  for (int q : adj[p]) {
    visit(q, mask, adj, visited);
  }
}

int enumerate_connected_subgraph_naive(int n, const std::vector<std::pair<int, int>>& graph) {
  std::vector<std::vector<int>> adj(n);

  for (auto& e : graph) {
    int u = e.first;
    int v = e.second;
    adj[u].push_back(v);
    adj[v].push_back(u);
  }

  int ret = 0;
  for (int mask = 0; mask < (1 << n); ++mask) {
    std::vector<bool> visited(n, false);
    int n_connected_component = 0;
    for (int i = 0; i < n; ++i) {
      if (((mask >> i) & 1) && !visited[i]) {
        ++n_connected_component;
        visit(i, mask, adj, visited);
      }
    }

    if (n_connected_component <= 1) {
      ++ret;
    }
  }

  return ret;
}

void connected_subgraph_test_path(int n) {
  std::vector<std::pair<int, int>> graph;
  for (int i = 0; i < n - 1; ++i) {
    graph.push_back({i, i + 1});
  }
  int expected = n * (n + 1) / 2 + 1;

  assert(enumerate_connected_subgraph_by_sat(n, graph) == expected);
}

void connected_subgraph_test_cycle(int n) {
  std::vector<std::pair<int, int>> graph;
  graph.push_back({0, n - 1});
  for (int i = 0; i < n - 1; ++i) {
    graph.push_back({i, i + 1});
  }
  int expected = n * (n - 1) + 2;

  assert(enumerate_connected_subgraph_by_sat(n, graph) == expected);
}

void connected_subgraph_test_any(int n, const std::vector<std::pair<int, int>>& graph) {
  int by_sat = enumerate_connected_subgraph_by_sat(n, graph);
  int naive = enumerate_connected_subgraph_naive(n, graph);

  assert(by_sat == naive);
}

void connected_subgraph_test_propagate_on_init() {
  {
    CaDiCaL::Solver solver;
    solver.set("chrono", 0);

    solver.add(1); solver.add(0);
    solver.add(3); solver.add(0);

    solver.add_extra(std::make_unique<CaDiCaL::SubgraphConnectivity>(
      std::vector<int>{1, 2, 3},
      std::vector<std::pair<int, int>>{{0, 1}, {1, 2}}
    ));

    assert(solver.solve() == 10);
    assert(solver.val(2) > 0);
  }

  {
    CaDiCaL::Solver solver;
    solver.set("chrono", 0);

    solver.add(1); solver.add(0);
    solver.add(-2); solver.add(0);
    solver.add(3); solver.add(0);

    solver.add_extra(std::make_unique<CaDiCaL::SubgraphConnectivity>(
      std::vector<int>{1, 2, 3},
      std::vector<std::pair<int, int>>{{0, 1}, {1, 2}}
    ));

    assert(solver.solve() == 20);
  }
}

}

int main() {
  connected_subgraph_test_path(1);
  connected_subgraph_test_path(2);
  connected_subgraph_test_path(5);
  connected_subgraph_test_path(50);

  connected_subgraph_test_cycle(1);
  connected_subgraph_test_cycle(2);
  connected_subgraph_test_cycle(5);
  connected_subgraph_test_cycle(50);

  connected_subgraph_test_any(9, {
    {0, 1},
    {1, 2},
    {3, 4},
    {4, 5},
    {6, 7},
    {7, 8},
    {0, 3},
    {1, 4},
    {2, 5},
    {3, 6},
    {4, 7},
    {5, 8},
  });

  connected_subgraph_test_propagate_on_init();

  return 0;
}
