/*
 * instance.hpp
 *
 */

#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <boost/date_time/posix_time/posix_time.hpp>

#include "lattice.hpp"
#include "walk.hpp"
#include "my_array.hpp"
#include "multiplicity.hpp"
#include "radius.hpp"
#include "flatperm.hpp"

#include "hdf5_hl.h"

#include <fstream>
#include <random>

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////

struct instance
{
  using lattice = lattices::triangular;
  using point = lattice::point;

  const unsigned int N;
  const double mu;

  // this is the random generator that flatperm will use
  // it's instantiated here, passed to the flatperm instance as a reference and
  // (IMPORTANT) seeded from main, i.e. you have to set the seed yourself.
  using random_generator_type = std::mt19937;
  random_generator_type rng;

  static const int num_flatperm_indices = 2;
  using flatperm_type = algorithm::flatperm<num_flatperm_indices, random_generator_type>;
  flatperm_type flatperm;

  using walk_type = models::walk<lattice>;
  walk_type walk;

  uint64_t samples;
  features::radius<point> radius;
  features::multiplicity<walk_type> multiplicity;
  my_array<long double, num_flatperm_indices> Re2W, Rg2W, Rm2W;

  my_array<long double, num_flatperm_indices - 1> sampled_weights;
  my_array<int, num_flatperm_indices + 1> sampled_walks;

  boost::posix_time::ptime start_time;

  //////////////////////////////////////////////////////////////////////
  instance(unsigned int N, double mu)
    : N(N), mu(mu)
    // initialise flatperm and pass the indices limits
    // of course to accommodate both length 0 and length N, the index must be
    // able to take N+1 possible values
    , flatperm({N+1, N/2+1}, mu, rng)
    , walk(N)
    , samples(0)
    // initialise out histogram with the dimensions as the flatperm histograms
    , Re2W{flatperm.extents}
    , Rg2W{flatperm.extents}
    , Rm2W{flatperm.extents}
    , sampled_weights({flatperm.extents[1]})
    , sampled_walks  ({flatperm.extents[1], flatperm.extents[0], 2})
  {
  }

  //
  // This constructor is used to resume a previous simulation
  // It reads some parameters from the datafile, initialises flatperm,
  // and reload the previous histograms
  // NOTE: be aware that we are not able to preserve the state of the random
  // number generator
  //
  instance(hdf5::handle loc)
    : instance( get_attribute(loc, "N") .read<unsigned int>(),
                get_attribute(loc, "mu").read<double>() )
  {
    flatperm.load(loc);
    std::cerr << "loading supplementary histograms: ";
    std::cerr << "Re2W, "; hdf5::load(loc, Re2W, "Re2W");
    std::cerr << "Rg2W, "; hdf5::load(loc, Rg2W, "Rg2W");
    std::cerr << "Rm2W\n"; hdf5::load(loc, Rm2W, "Rm2W");
  }

  void print_stats() const
  {
    auto const now = boost::posix_time::second_clock::local_time();
    double seconds = (double) (now - start_time).total_milliseconds()
      / 1000;

    uint64_t tours = flatperm.Sn.data()[0];

    std::cerr << "check point at time " << now << "\n"
        << tours << " tours "
        << " (" << (double) tours / seconds << " tours/sec) "
        << samples << " samples"
        << " (" << (double) samples / seconds << " samples/sec)\n";
  }

  void save(hdf5::handle loc) const
  {
    // save preliminary info on the datafile
    hid_t loc_id = loc.getId();

    H5LTset_attribute_string(loc_id, ".", "TITLE", PACKAGE);
    H5LTset_attribute_uint  (loc_id, ".", "N", &N, 1);
    H5LTset_attribute_double(loc_id, ".", "mu", &mu, 1);

    flatperm.save(loc);

    std::cerr << "saving supplementary histograms: ";
    std::cerr << "Re2W, "; hdf5::save(loc, Re2W, "Re2W");
    std::cerr << "Rg2W, "; hdf5::save(loc, Rg2W, "Rg2W");
    std::cerr << "Rm2W, "; hdf5::save(loc, Rm2W, "Rm2W");

    auto const now = boost::posix_time::second_clock::local_time();

    std::string const time_str = to_simple_string(now);
    H5LTset_attribute_string(loc.getId(), ".", "time", time_str.c_str());

    std::cerr << "walks, "; hdf5::save(loc, sampled_walks, "sampled_walks");
    std::cerr << "weights\n"; hdf5::save(loc, sampled_weights, "sampled_weights");
  }

  std::vector<point> atmosphere() const
  {
    return walk.atmosphere();
  }

  //
  // Register a new step
  //
  // This is called from flatperm each time a new step has been decided.
  // This function must add that step to the walk, register it with any
  // counters and/or observables, and update the flatperm indices.
  // This function can also correct the weight of the sample.
  //
  void register_step(point const& x, long double& W)
  {
    samples ++;

    walk.register_step(x);
    radius.register_step(walk);
    multiplicity.register_step(walk);

    flatperm.indices[0] = walk.size();
    flatperm.indices[1] = multiplicity.get<2>();

    auto const n = walk.size();

    typedef decltype(radius.B) point_int64_t;
    long double const A = norm_square(point_int64_t(walk.back()));
    long double const B = radius.get_CM_norm_square();
    long double const C = radius.get_norm_square_sum();

    long double const Re2 = A;
    long double const Rg2 = C / n - B / n / n;
    long double const Rm2 = C / n;

    Re2W(flatperm.indices) += W * Re2;
    Rg2W(flatperm.indices) += W * Rg2;
    Rm2W(flatperm.indices) += W * Rm2;

    if (n == N) {
      auto m = flatperm.indices[1];
      if (W > sampled_weights[m]) {
        sampled_weights[m] = W;
        int i = 0;
        for(auto const& xy : walk) {
          sampled_walks[m][i][0] = xy[0];
          sampled_walks[m][i][1] = xy[1];
          i += 1;
        }
      }
    }
  }

  void unregister_step()
  {
    multiplicity.unregister_step(walk);
    radius.unregister_step(walk);
    walk.unregister_step();

    flatperm.indices[0] = walk.size();
    flatperm.indices[1] = multiplicity.get<2>();
  }

  void run(unsigned int S) {
    start_time = boost::posix_time::second_clock::local_time();
    return flatperm.run(this, S);
  }
};

#endif

/* vim: set et fenc=utf-8 ff=unix sts=0 sw=2 ts=2 : */
