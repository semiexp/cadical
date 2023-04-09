#include "../../src/cadical.hpp"
#include "../../src/extra_constraint.hpp"

namespace CaDiCaL {

// An implementation of clauses based on `ExtraConstraint` framework.
// This will be inefficient than native clauses and is intended only for testing purpose.
class ExtClause : public ExtraConstraint {
public:
  ExtClause(const std::vector<int>& elits) : elits_(elits) {}

  bool initialize(Internal& solver, std::vector<int>& need_watch) override {
    for (int elit : elits_) {
      int lit = solver.external->internalize(elit);
      lits_.push_back(lit);
      need_watch.push_back(-lit);
    }
    return true;
  }

  bool propagate(Internal& solver, int lit) override {
    int undet_lit = 0;
    for (int l : lits_) {
      signed char b = solver.val(l);
      if (b > 0) {
        // already satisfied
        return true;
      }
      if (b == 0) {
        // more than one undecided lit
        if (undet_lit != 0) {
          return true;
        }
        undet_lit = l;
      }
    }

    if (undet_lit != 0) {
      solver.search_assign_ext(undet_lit, this);
      return true;
    } else {
      return false;
    }
  }

  std::vector<int> calc_reason(Internal& solver, int lit) override {
    std::vector<int> ret;
    for (int l : lits_) {
      if (l == lit) {
        continue;
      }
      ret.push_back(-l);
    }
    return ret;
  }

  void undo(Internal&, int) override {}  // nothing to do

private:
  std::vector<int> elits_;
  std::vector<int> lits_;
};

}

namespace {

void run_check(const std::vector<std::vector<int>>& clauses, bool is_sat) {
  CaDiCaL::Solver solver;
  solver.set("chrono", 0);

  for (const std::vector<int>& clause : clauses) {
    solver.add_extra(std::make_unique<CaDiCaL::ExtClause>(clause));
  }

  int res = solver.solve();
  if (is_sat) {
    assert(res == 10);

    for (const std::vector<int>& clause : clauses) {
      bool is_sat = false;
      for (int lit : clause) {
        if (solver.val(lit) > 0) {
          is_sat = true;
          break;
        }
      }
      assert(is_sat);
    }
  } else {
    assert(res == 20);
  }
}

}

int main() {
  run_check({
    {1, 2},
    {1, -2},
    {-1, 2},
  }, true);

  run_check({
    {4, 1},
    {-4, -1},
    {2, 3},
    {-2, -3},
    {1, 2},
    {-1, -2},
    {3, 4},
    {-3, -4},
  }, true);

  run_check({
    {4, 5},
    {-4, -5},
    {2, 3},
    {-2, -3},
    {1, 2},
    {-1, -2},
    {3, 4},
    {-3, -4},
    {5, 1},
    {-5, -1},
  }, false);

  std::vector<std::vector<int>> instance_3sat = {
    {10, -2, 9},
    {10, -9, -8},
    {-4, -2, -6},
    {-6, -5, 8},
    {-9, 2, 7},
    {5, -9, 4},
    {-6, -4, 8},
    {-10, -7, -8},
    {-2, 3, 1},
    {3, -8, -1},
    {7, -2, -5},
    {1, -7, 4},
    {3, 8, -2},
    {-1, -9, 6},
    {-4, 5, 8},
    {2, -8, -5},
    {-5, -3, 8},
    {-7, -1, -10},
    {-8, 1, 7},
    {-9, -2, -7},
    {-2, -8, -6},
    {10, -3, 2},
    {-1, 8, -3},
    {-4, -8, 7},
    {8, -4, 7},
    {2, 9, -8},
    {-1, -10, -8},
    {6, 10, -1},
    {-4, -6, 10},
    {9, 2, 1},
    {4, -3, 1},
    {-3, -6, 9},
    {10, -7, 8},
    {-10, -9, -5},
    {-2, -7, -10},
    {-8, 5, -7},
    {8, -5, -1},
    {5, 6, 9},
    {1, -3, 6},
    {-5, 8, 6},
    {-9, 5, -6},
    {6, 5, -8},
    {9, 2, -4},
    {-6, 4, 7},
  };

  run_check(instance_3sat, false);

  instance_3sat.pop_back();
  run_check(instance_3sat, true);

  return 0; 
}
