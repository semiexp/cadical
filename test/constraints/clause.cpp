#include "../../src/cadical.hpp"
#include "../../src/extra_constraint.hpp"

#include <set>
#include <random>

namespace CaDiCaL {

// An implementation of clauses based on `ExtraConstraint` framework.
// This will be inefficient than native clauses and is intended only for testing purpose.
class ExtClause : public ExtraConstraint {
public:
  ExtClause(const std::vector<int>& elits) : elits_(elits) {}

  bool initialize(Internal& solver) override {
    for (int elit : elits_) {
      int lit = solver.external->internalize(elit);
      solver.require_extra_watch(-lit, this);
      lits_.push_back(lit);
    }

    std::vector<int> prop_list;
    for (int lit : lits_) {
      signed char v = solver.val(lit);
      if (v == -1) {
        prop_list.push_back(-lit);
      }
    }

    for (int lit : prop_list) {
      if (!propagate(solver, lit)) {
        return false;
      }
    }

    return true;
  }

  bool propagate(Internal& solver, int lit) override {
    for (int l : assignment_stack_) {
      assert(l != lit);
    }
    assignment_stack_.push_back(lit);

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
        assert(solver.val_analyze(l) == 0);
        continue;
      }
      ret.push_back(-l);
      assert(solver.val_analyze(l) == -1);
    }
    return ret;
  }

  void undo(Internal&, int lit) override {
    assert(!assignment_stack_.empty());
    assert(assignment_stack_.back() == lit);
    assignment_stack_.pop_back();
  }

private:
  std::vector<int> elits_;
  std::vector<int> lits_;

  std::vector<int> assignment_stack_;  // for debug
};

// The same semantics as `ExtClause`, but this propagets "lazily":
// values are recognized only after being notified by `propagate`.
class LazyExtClause : public ExtraConstraint {
public:
  LazyExtClause(const std::vector<int>& elits) :
    elits_(elits),
    n_undet_(elits.size()),
    n_sat_(0),
    prop_fail_(0),
    is_assigned_(elits.size(), false) {}

  bool initialize(Internal& solver) override {
    for (int elit : elits_) {
      int lit = solver.external->internalize(elit);
      lits_.push_back(lit);
      solver.require_extra_watch(-lit, this);
      solver.require_extra_watch(lit, this);
    }

    // We first collect the list of literals which have already been assigned
    // for calling `propagate`.
    // We should NOT call `propagate` immediately after checking values, because
    // this may result in calling `propagate` for literals decided in `propagate` again.
    std::vector<int> prop_list;
    for (int lit : lits_) {
      signed char v = solver.val(lit);
      if (v == 1) {
        prop_list.push_back(lit);
      } else if (v == -1) {
        prop_list.push_back(-lit);
      }
    }

    for (int lit : prop_list) {
      if (!propagate(solver, lit)) {
        return false;
      }
    }

    return true;
  }

  bool propagate(Internal& solver, int lit) override {
    for (int l : assignment_stack_) {
      assert(l != lit && l != -lit);
    }

    assignment_stack_.push_back(lit);
    int idx = literal_index(lit);
    if (idx >= 0) {
      ++n_sat_;
      assert(!is_assigned_[idx]);
      is_assigned_[idx] = true;
    } else {
      assert(!is_assigned_[~idx]);
      is_assigned_[~idx] = true;
    }

    assert(n_undet_ > 0);
    --n_undet_;

    if (n_sat_ > 0) {
      return true;
    }

    prop_fail_ = 0;

    if (n_undet_ == 0) {
      return false;
    } else if (n_undet_ == 1) {
      int p = 0;
      for (size_t i = 0; i < lits_.size(); ++i) {
        if (!is_assigned_[i]) {
          assert(p == 0);
          p = lits_[i];
        }
      }
      assert(p != 0);

      // Even in "lazy" propagators like this, we should check the value of
      // the literal to be assigned, because `search_assign_ext` expects that
      // the given literal is not assigned yet. If this assignment fails
      // because the literal is already set to false, we should also include
      // the literal to the reason, so we memorize it (`prop_fail_`).
      signed char v = solver.val(p);
      if (v == 1) {
        return true;
      } else if (v == 0) {
        solver.search_assign_ext(p, this);
        return true;
      } else {
        prop_fail_ = p;
        return false;
      }
    } else {
      return true;
    }
  }

  std::vector<int> calc_reason(Internal& solver, int lit) override {
    assert(n_sat_ == 0);
    assert(n_undet_ <= 1);

    std::vector<int> ret;

    if (n_undet_ == 0) {
      assert(lit == 0);

      for (int l : lits_) {
        ret.push_back(-l);
      }
    } else {
      if (prop_fail_ == 0) {
        assert(lit != 0);
      } else {
        assert(lit == 0);
        ret.push_back(-prop_fail_);
      }
      for (size_t i = 0; i < lits_.size(); ++i) {
        if (is_assigned_[i]) {
          ret.push_back(-lits_[i]);
        }
      }
    }

    for (int l : ret) {
      assert(solver.val_analyze(l) == 1);
    }

    return ret;
  }

  void undo(Internal&, int lit) override {
    assert(!assignment_stack_.empty());
    assert(assignment_stack_.back() == lit);
    assignment_stack_.pop_back();

    int idx = literal_index(lit);
    if (idx >= 0) {
      assert(n_sat_ > 0);
      --n_sat_;
      assert(is_assigned_[idx]);
      is_assigned_[idx] = false;
    } else {
      assert(is_assigned_[~idx]);
      is_assigned_[~idx] = false;
    }

    ++n_undet_;
    prop_fail_ = 0;
  }

private:
  int literal_index(int lit) const {
    for (int i = 0; i < (int)lits_.size(); ++i) {
      if (lits_[i] == lit) {
        return i;
      } else if (lits_[i] == -lit) {
        return ~i;
      }
    }
    assert(false);
  }

  std::vector<int> elits_;
  std::vector<int> lits_;
  int n_undet_, n_sat_, prop_fail_;

  std::vector<int> assignment_stack_;
  std::vector<bool> is_assigned_;
};

}

namespace {

void run_check(const std::vector<std::vector<int>>& clauses, bool is_sat) {
  for (bool use_lazy : {false, true}) {
    CaDiCaL::Solver solver;
    solver.set("chrono", 0);

    if (use_lazy) {
      for (const std::vector<int>& clause : clauses) {
        solver.add_extra(std::make_unique<CaDiCaL::LazyExtClause>(clause));
      }
    } else {
      for (const std::vector<int>& clause : clauses) {
        solver.add_extra(std::make_unique<CaDiCaL::ExtClause>(clause));
      }
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

void compare_large_sat(int seed, int nvar) {
  std::mt19937 rng(seed);

  CaDiCaL::Solver solver;
  CaDiCaL::Solver ext_solver;
  CaDiCaL::Solver lazy_ext_solver;
  ext_solver.set("chrono", 0);
  lazy_ext_solver.set("chrono", 0);

  for (;;) {
    std::set<int> vars;
    int clause_size = std::uniform_int_distribution<int>(2, 5)(rng);
    while ((int)vars.size() < clause_size) {
      int v = std::uniform_int_distribution<int>(1, nvar)(rng);
      vars.insert(v);
    }

    std::vector<int> clause;
    for (int v : vars) {
      int sign = std::uniform_int_distribution<int>(0, 1)(rng) * 2 - 1;
      clause.push_back(v * sign);
    }

    for (int lit : clause) {
      solver.add(lit);
    }
    solver.add(0);
    int res_solver = solver.solve();
    assert(res_solver == 10 || res_solver == 20);

    ext_solver.add_extra(std::make_unique<CaDiCaL::ExtClause>(clause));
    int res_ext_solver = ext_solver.solve();
    assert(res_solver == res_ext_solver);

    lazy_ext_solver.add_extra(std::make_unique<CaDiCaL::LazyExtClause>(clause));
    int res_lazy_ext_solver = lazy_ext_solver.solve();
    assert(res_solver == res_lazy_ext_solver);

    if (res_solver == 20) {
      break;
    }
  }
}

void test_propagate_on_init() {
  for (bool use_lazy : {false, true}) {
    {
      CaDiCaL::Solver solver;
      solver.set("chrono", 0);

      solver.add(1); solver.add(0);
      solver.add(2); solver.add(0);

      if (use_lazy) {
        solver.add_extra(std::make_unique<CaDiCaL::LazyExtClause>(std::vector<int>{-1, -2}));
      } else {
        solver.add_extra(std::make_unique<CaDiCaL::ExtClause>(std::vector<int>{-1, -2}));
      }
      // TODO: ensure that conflict is detected at this timing before calling `solve()`

      assert(solver.solve() == 20);
    }

    {
      CaDiCaL::Solver solver;
      solver.set("chrono", 0);

      solver.add(1); solver.add(0);
      solver.add(2); solver.add(0);

      if (use_lazy) {
        solver.add_extra(std::make_unique<CaDiCaL::LazyExtClause>(std::vector<int>{-1, -2, -3}));
      } else {
        solver.add_extra(std::make_unique<CaDiCaL::ExtClause>(std::vector<int>{-1, -2, -3}));
      }
      // TODO: ensure that val(3) is decided at this timing before calling `solve()`

      assert(solver.solve() == 10);
      assert(solver.val(3) < 0);
    }
  }
}

}

int main() {
  test_propagate_on_init();

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

  for (int seed : {37, 42, 100}) {
    for (int nvar : {20, 50, 100, 200}) {
      compare_large_sat(seed, nvar);
    }
  }

  return 0; 
}
