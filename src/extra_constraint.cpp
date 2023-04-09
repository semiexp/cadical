#include "internal.hpp"

namespace CaDiCaL {

bool Internal::add_extra(std::unique_ptr<ExtraConstraint>&& constr, const std::vector<int>& need_watch) {
  // TODO: extra constraints are not guaranteed to work with chronological backtracking
  assert(opts.chrono == 0);

  if (level) backtrack ();

  ext_constr.push_back(std::move(constr));
  ExtraConstraint* constr_ptr = ext_constr.back().get();

  for (int lit : need_watch) {
    ext_watches(lit).push_back(constr_ptr);
    freeze(lit);
  }

  return true;
}

}
