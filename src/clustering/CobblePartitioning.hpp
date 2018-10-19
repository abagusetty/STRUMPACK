/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The
 * Regents of the University of California, through Lawrence Berkeley
 * National Laboratory (subject to receipt of any required approvals
 * from the U.S. Dept. of Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As
 * such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research
 *             Division).
 *
 */
#ifndef COBBLE_PARTITIONING_HPP
#define COBBLE_PARTITIONING_HPP

#include <vector>
#include <random>

#include "dense/DenseMatrix.hpp"
#include "HSS/HSSPartitionTree.hpp"
#include "NeighborSearch.hpp"

namespace strumpack {

  template<typename scalar_t,
           typename real_t=typename RealType<scalar_t>::value_type>
  void cobble_partition
  (DenseMatrix<scalar_t>& p, std::vector<std::size_t>& nc,
   DenseMatrix<scalar_t>& labels) {
    auto d = p.rows();
    auto n = p.cols();
    auto nl = labels.rows();
    // find centroid
    std::vector<scalar_t> centroid(d);
    for (std::size_t i=0; i<n; i++)
      for (std::size_t j=0; j<d; j++)
        centroid[j] += p(j, i);
    for (std::size_t j=0; j<d; j++)
      centroid[j] /= n;

    // find farthest point from centroid
    std::size_t first_index = 0;
    real_t max_dist(-1);
    for (std::size_t i=0; i<n; i++) {
      auto dd = Euclidean_distance(d, p.ptr(0, i), centroid.data());
      if (dd > max_dist) {
        max_dist = dd;
        first_index = i;
      }
    }

    // compute and sort distance from the firsth point
    std::vector<real_t> dists(n);
    for (std::size_t i=0; i<n; i++)
      dists[i] = Euclidean_distance(d, p.ptr(0, i), p.ptr(0, first_index));

    std::vector<std::size_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::nth_element
      (idx.begin(), idx.begin() + n/2, idx.end(),
       [&](const std::size_t& a, const std::size_t& b) {
        return dists[a] < dists[b];
      });

    // split the data
    nc[0] = n/2;
    nc[1] = n - n/2;
    std::vector<int> cluster(n);
    for (std::size_t i=0; i<n/2; i++)
      cluster[idx[i]] = 0;
    for (std::size_t i=n/2; i<n; i++)
      cluster[idx[i]] = 1;

    // permute the data
    std::size_t ct = 0;
    for (std::size_t j=0, cj=ct; j<nc[0]; j++) {
      while (cluster[cj] != 0) cj++;
      if (cj != ct) {
        blas::swap(d, p.ptr(0,cj), 1, p.ptr(0,ct), 1);
        blas::swap(nl, labels.ptr(0,cj), 1, labels.ptr(0,ct), 1);
        cluster[cj] = cluster[ct];
        cluster[ct] = 0;
      }
      cj++;
      ct++;
    }
  }

  template<typename scalar_t> void recursive_cobble
  (DenseMatrix<scalar_t>& p, std::size_t cluster_size,
   HSS::HSSPartitionTree& tree, DenseMatrix<scalar_t>& labels) {
    auto n = p.cols();
    auto d = p.rows();
    auto l = labels.rows();
    if (n < cluster_size) return;
    std::vector<std::size_t> nc(2);
    cobble_partition(p, nc, labels);
    if (!nc[0] || !nc[1]) return;
    tree.c.resize(2);
    tree.c[0].size = nc[0];
    tree.c[1].size = nc[1];
    {
      DenseMatrixWrapper<scalar_t> p0(d, nc[0], p, 0, 0);
      DenseMatrixWrapper<scalar_t> l0(l, nc[0], labels, 0, 0);
      recursive_cobble(p0, cluster_size, tree.c[0], l0);
    } {
      DenseMatrixWrapper<scalar_t> p1(d, nc[1], p, 0, nc[0]);
      DenseMatrixWrapper<scalar_t> l1(l, nc[1], labels, 0, nc[0]);
      recursive_cobble(p1, cluster_size, tree.c[1], l1);
    }
  }

} // end namespace strumpack

#endif // COBBLE_PARTITIONING_HPP
