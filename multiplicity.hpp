/*
 * multiplicity.hpp
 * 
 */

#ifndef MULTIPLICITY_HPP
#define MULTIPLICITY_HPP

#include <array>
#include <cassert>
#include <unordered_set>

namespace features {
  template<class Walk>
  class multiplicity {
    using lattice_type = typename Walk::lattice_type;
    using point = typename lattice_type::point;

    std::unordered_multiset<point, typename lattice_type::hash> _points;

    // this should depend on coordination
    std::array<unsigned int, lattice_type::coordination/2+1> _m;

  public:
    multiplicity() {
      _m.fill(0);
    }

    void register_step(Walk const& walk) {
      _points.insert(walk.back());
      int k = _points.count(walk.back());
      assert(k > 0);
      _m[k-1] --;
      _m[k] ++;
    }

    void unregister_step(Walk const& walk) {
      auto it = _points.find(walk.back());
      assert(it != _points.end());

      int k = _points.count(walk.back());
      assert(k > 0);
      _m[k] --;
      _m[k-1] ++;

      _points.erase(it);
    }

    template<unsigned int I>
    unsigned int get() const { return _m[I]; }

    friend std::ostream& operator<<(std::ostream& o, multiplicity const& mult) {
      for (auto x : mult._m)
        o << x << " ";
      return o;
    }
  };
}

#endif
