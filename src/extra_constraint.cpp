#include "internal.hpp"

namespace CaDiCaL {

void Internal::add_extra(std::unique_ptr<ExtraConstraint>&& constr) {
  // TODO: extra constraints are not guaranteed to work with chronological backtracking
  assert(opts.chrono == 0);

  if (level) backtrack ();

  ext_constr.push_back(std::move(constr));
  ExtraConstraint* constr_ptr = ext_constr.back().get();

  if (!constr_ptr->initialize(*this)) {
    learn_empty_clause();
    return;
  }

  if (propagate ()) return;
  LOG ("propagation of an extra constraint results in conflict");
  learn_empty_clause();
}

void Internal::require_extra_watch(int lit, ExtraConstraint* constr) {
  ext_watches(lit).push_back(constr);
  external->freeze(externalize(lit));
}

}
