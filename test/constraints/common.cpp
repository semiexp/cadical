#include "common.hpp"

int count_sat_assignment(CaDiCaL::Solver& solver, const std::vector<int>& vars) {
  int cnt = 0;

  while (true) {
    int is_sat = solver.solve();
    if (is_sat == 20) {
      break;
    }

    ++cnt;

    std::vector<int> refutation;
    for (int v : vars) {
      refutation.push_back(-solver.val(v));
    }

    for (int r : refutation) {
      solver.add(r);
    }
    solver.add(0);
  }

  return cnt;
}
