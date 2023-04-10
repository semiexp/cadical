#ifndef _extra_constraint_hpp_INCLUDED
#define _extra_constraint_hpp_INCLUDED

#include "internal.hpp"

namespace CaDiCaL {

// The abstract class for representing extra (non-clausal) constraints.
//
// As suggested in the MiniSat paper [Een, Sorensson, 2004], constraints in
// a SAT solver are not limited to clausal ones.
//
// Conceptually, an `ExtraConstraint` can be seen as a collection of
// (typically too many) clauses. Any literal appearing in these (virtual)
// clauses are "related" and should be added to `need_watch` in `initialize`.
class ExtraConstraint {
public:
  ExtraConstraint() {}
  virtual ~ExtraConstraint() = default;

  // Initialize the extra constraint with `solver`. This method should:
  // - push related literals to `need_watch`,
  // - retrieve already assigned values to propagate them, and
  // - return `true` if no conflict detected, or `false` otherwise.
  // This method should NOT update watches of related literals by itself,
  // because these literals are constrained in an unpredictable way to
  // the preprocessor, thus should be "frozen".
  virtual bool initialize(Internal& solver) = 0;

  virtual bool propagate(Internal& solver, int lit) = 0;

  // Compute the reason why the literal `lit` is derived. That is, under
  // this constraint, `lit` should be derived under assumption that
  // all literals in the returned reason are true.
  virtual std::vector<int> calc_reason(Internal& solver, int lit) = 0;

  virtual void undo(Internal& solver, int lit) = 0;
};

}

#endif
