/*
 * walk.hpp
 *
 */

#ifndef WALK_HPP__
#define WALK_HPP__

#include <cassert>
#include <iterator>
#include <iostream>
#include <vector>
#include <unordered_map>

namespace models {
  template<typename Lattice>
  class walk {
  public:
    typedef Lattice lattice_type;
    typedef typename lattice_type::point point;

  private:
    std::unordered_map<point, std::vector<std::pair<int, int>>, typename lattice_type::hash> _cuts;
    std::vector<point> _walk;

    using value_type = typename std::vector<point>::value_type;
    using iterator = typename std::vector<point>::iterator;
    using const_iterator = typename std::vector<point>::const_iterator;
    using const_reverse_iterator = typename std::vector<point>::const_reverse_iterator;

  public:
    walk(unsigned int N = 0)
    {
      _walk.reserve(N + 1);
      _walk.push_back(lattice_type::origin());
      // this is enough to default construct _cuts[(0,0)]
      _cuts[lattice_type::origin()];
    }

    std::size_t size() const
    {
      return _walk.size() - 1;
    }

    point const& operator[](std::size_t i) const
    {
      return _walk[i];
    }

    const_iterator begin() const { return _walk.begin(); }
    const_iterator end()   const { return _walk.end(); }

    const_reverse_iterator rbegin() const { return _walk.rbegin(); }
    const_reverse_iterator rend()  const { return _walk.rend();  }

    value_type front() const { return _walk.front(); }
    value_type back()  const { return _walk.back(); }

    //////////////////////////////////////////////////////////////////////

    bool check_step(point z) const
    {
      if (_walk.size() < 1)
        return true;

      auto y = _walk[_walk.size()-1];

      // we have already closed on ourself, no further steps are possible
      if (y == lattice_type::origin() and z == _walk[1])
        return false;

      if (_walk.size() < 2)
        return true;

      auto x = _walk[_walk.size()-2];

      int j = segment_code(y, z);
      int k = segment_code(y, x);
      if (j > k) std::swap(k, j);

//       std::cerr << "[check] points " << x << " " << y << " " << z
//         << " would enter from " << j << " and exit through " << k;

      if (j == k) {
//         std::cerr << " no good (backstep)\n";
        return false;
      }

      auto it = _cuts.find(y);
      if (it == _cuts.end() or it->second.empty()) {
//         std::cerr << " good (not visited)\n";
        return true;
      }

      auto& cuts_y = it->second;

//       std::cerr << " cuts: ";
//       for (auto i : cuts_y)
//         std::cerr << i.first << "-" << i.second << " ";

      for (auto i : cuts_y) {
        auto a = i.first, b = i.second;
        if (!((a < j and k < b) or (j < a and b < k))) {
//           std::cerr << " no good (crosses or segment already visited)\n";
          return false;
        }
      }

//       std::cerr << " good\n";
      return true;
    }

    static int segment_code(point x, point y) {
      auto delta = y - x;

      if (delta == point{-1, 0}) return 0;
      if (delta == point{ 0, 1}) return 1;
      if (delta == point{ 1, 1}) return 2;
      if (delta == point{ 1, 0}) return 3;
      if (delta == point{ 0,-1}) return 4;
      if (delta == point{-1,-1}) return 5;
      abort();
    }

    void register_step(point z)
    {
      if (_walk.size() > 1) {
        auto y = _walk[_walk.size()-1];
        auto x = _walk[_walk.size()-2];

        int j = segment_code(y, z);
        int k = segment_code(y, x);
        if (j > k) std::swap(k, j);

        _cuts[y].push_back({j, k});

//         std::cout << "points " << x << " " << y << " " << z
//           << " enters from " << j << " and exits through " << k;
//
//         std::cerr << " cuts: ";
//         for (auto i : _cuts[y])
//           std::cerr << i.first << "-" << i.second << " ";
//         std::cerr << "\n";
      }
      _walk.push_back(z);
    }

    void unregister_step()
    {
      assert(not _walk.empty());
      if (_walk.size() > 2) {
        auto z = _walk[_walk.size()-1];
        auto y = _walk[_walk.size()-2];
        auto x = _walk[_walk.size()-3];

        int j = segment_code(y, z);
        int k = segment_code(y, x);
        if (k > j) std::swap(k, j);

        auto& cuts_y = _cuts[y];
        auto it = find(cuts_y.begin(), cuts_y.end(), std::make_pair(k, j));
        assert(it != cuts_y.end());
        cuts_y.erase(it);
      }
      _walk.pop_back();
    }

    std::vector<point> atmosphere() const
    {
      std::vector<point> atmosphere;
      atmosphere.reserve(lattice_type::coordination);
      for (auto x: lattice_type::get_neighbours(back())) {
        if (check_step(x))
          atmosphere.push_back(x);
      };
      return atmosphere;
    }

    friend
    std::ostream& operator<<(std::ostream& o, walk const& walk)
    {
      auto i = std::begin(walk);
      while (true) {
        o << *i++;
        if (i == std::end(walk))
          break;
        o << " ";
      }
      return o;
    }
  };
}

#endif // WALK_HPP__

/* vim: set et fenc=utf-8 ff=unix sts=0 sw=2 ts=2 : */
