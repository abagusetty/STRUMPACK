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
#include <iostream>
#include <algorithm>

#include "FrontFactory.hpp"

#include "sparse/CSRGraph.hpp"
#include "FrontalMatrixDense.hpp"
#include "FrontHSS.hpp"
#include "FrontBLR.hpp"
#if defined(STRUMPACK_USE_BPACK)
#include "FrontHODLR.hpp"
#endif
#if defined(STRUMPACK_USE_MPI)
#include "FrontalMatrixDenseMPI.hpp"
#include "FrontHSSMPI.hpp"
#include "FrontBLRMPI.hpp"
#if defined(STRUMPACK_USE_BPACK)
#include "FrontHODLRMPI.hpp"
#endif
#endif
#if defined(STRUMPACK_USE_MAGMA)
#include "FrontMAGMA.hpp"
#endif
#if defined(STRUMPACK_USE_GPU)
#include "FrontalMatrixGPU.hpp"
#endif
#if defined(STRUMPACK_USE_CUDA)
#include "FrontGPUSPD.hpp"
#endif
#if defined(STRUMPACK_USE_ZFP)
#include "FrontLossy.hpp"
#endif
#if defined(STRUMPACK_USE_H2OPUS)
#include "FrontH2Opus.hpp"
#endif

namespace strumpack {

  template<typename scalar_t, typename integer_t>
  std::unique_ptr<FrontalMatrix<scalar_t,integer_t>> create_frontal_matrix
  (const SPOptions<scalar_t>& opts, integer_t s, integer_t sbegin,
   integer_t send, std::vector<integer_t>& upd,
   int level, FrontCounter& fc, bool root) {
    auto dsep = send - sbegin;
    auto dupd = upd.size();
    std::unique_ptr<FrontalMatrix<scalar_t,integer_t>> front;
    switch (opts.compression()) {
    case CompressionType::NONE: {
      // see below
    } break;
    case CompressionType::HSS: {
      if (is_HSS(dsep, dupd, opts)) {
        front = std::make_unique<FrontHSS<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.HSS++;
      }
    } break;
    case CompressionType::BLR: {
      if (is_BLR(dsep, dupd, opts)) {
        front = std::make_unique<FrontBLR<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.BLR++;
      }
    } break;
    case CompressionType::HODLR: {
#if defined(STRUMPACK_USE_BPACK)
      if (is_HODLR(dsep, dupd, opts)) {
        front = std::make_unique<FrontHODLR<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.HODLR++;
      }
#endif
    } break;
    case CompressionType::H2: {
#if defined(STRUMPACK_USE_H2OPUS)
      if (is_H2(dsep, dupd, opts)) {
        front = std::make_unique<FrontH2Opus<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.H2++;
      }
#endif
    } break;
    case CompressionType::BLR_HODLR: {
#if defined(STRUMPACK_USE_BPACK)
      if (is_HODLR(dsep, dupd, opts, 0)) {
        front = std::make_unique<FrontHODLR<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.HODLR++;
      }
#endif
      if (!front && is_BLR(dsep, dupd, opts, 1)) {
        front = std::make_unique<FrontBLR<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.BLR++;
      }
    } break;
    case CompressionType::ZFP_BLR_HODLR: {
#if defined(STRUMPACK_USE_BPACK)
      if (is_HODLR(dsep, dupd, opts, 0)) {
        front = std::make_unique<FrontHODLR<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.HODLR++;
      }
#endif
      if (!front && is_BLR(dsep, dupd, opts, 1)) {
        front.reset
          (new FrontBLR<scalar_t,integer_t>(s, sbegin, send, upd));
        if (root) fc.BLR++;
      }
#if defined(STRUMPACK_USE_ZFP)
      if (!front && is_lossy(dsep, dupd, opts, 2)) {
        front = std::make_unique<FrontLossy<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.lossy++;
      }
#endif
    } break;
    case CompressionType::LOSSLESS:
    case CompressionType::LOSSY: {
#if defined(STRUMPACK_USE_ZFP)
      if (is_lossy(dsep, dupd, opts)) {
        front = std::make_unique<FrontLossy<scalar_t,integer_t>>
          (s, sbegin, send, upd);
        if (root) fc.lossy++;
      }
#endif
    } break;
    };
    if (front) return front;
    if (is_GPU(opts)) {
#if defined(STRUMPACK_USE_CUDA)
      if (is_symmetric(opts) && is_positive_definite(opts))
        front = std::make_unique<FrontGPUSPD<scalar_t,integer_t>>
          (s, sbegin, send, upd);
      else
#endif
        {
#if defined(STRUMPACK_USE_MAGMA)
        front = std::make_unique<FrontMAGMA<scalar_t,integer_t>>
          (s, sbegin, send, upd);
#elif defined(STRUMPACK_USE_GPU)
      front = std::make_unique<FrontalMatrixGPU<scalar_t,integer_t>>
        (s, sbegin, send, upd);
#endif
        }
      if (root && front) fc.dense++;
    }
    if (front) return front;
    // fallback in case support for cublas/zfp/hodlr is missing
    front = std::make_unique<FrontalMatrixDense<scalar_t,integer_t>>
      (s, sbegin, send, upd);
    if (root) fc.dense++;
    return front;
  }


  // explicit template instantiations
  template std::unique_ptr<FrontalMatrix<float,int>>
  create_frontal_matrix(const SPOptions<float>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<double,int>>
  create_frontal_matrix(const SPOptions<double>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<std::complex<float>,int>>
  create_frontal_matrix(const SPOptions<std::complex<float>>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<std::complex<double>,int>>
  create_frontal_matrix(const SPOptions<std::complex<double>>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc, bool root);

  template std::unique_ptr<FrontalMatrix<float,long int>>
  create_frontal_matrix(const SPOptions<float>& opts, long int s, long int sbegin,
                        long int send, std::vector<long int>& upd,
                        int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<double,long int>>
  create_frontal_matrix(const SPOptions<double>& opts, long int s, long int sbegin,
                        long int send, std::vector<long int>& upd,
                        int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<std::complex<float>,long int>>
  create_frontal_matrix(const SPOptions<std::complex<float>>& opts, long int s,
                        long int sbegin, long int send, std::vector<long int>& upd,
                        int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<std::complex<double>,long int>>
  create_frontal_matrix(const SPOptions<std::complex<double>>& opts, long int s,
                        long int sbegin, long int send, std::vector<long int>& upd,
                        int level, FrontCounter& fc, bool root);

  template std::unique_ptr<FrontalMatrix<float,long long int>>
  create_frontal_matrix(const SPOptions<float>& opts, long long int s, long long int sbegin,
                        long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<double,long long int>>
  create_frontal_matrix(const SPOptions<double>& opts, long long int s, long long int sbegin,
                        long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<std::complex<float>,long long int>>
  create_frontal_matrix(const SPOptions<std::complex<float>>& opts, long long int s,
                        long long int sbegin, long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, bool root);
  template std::unique_ptr<FrontalMatrix<std::complex<double>,long long int>>
  create_frontal_matrix(const SPOptions<std::complex<double>>& opts, long long int s,
                        long long int sbegin, long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, bool root);


#if defined(STRUMPACK_USE_MPI)
  template<typename scalar_t, typename integer_t>
  std::unique_ptr<FrontalMatrixMPI<scalar_t,integer_t>> create_frontal_matrix
  (const SPOptions<scalar_t>& opts, integer_t s,
   integer_t sbegin, integer_t send, std::vector<integer_t>& upd,
   int level, FrontCounter& fc, const MPIComm& comm, int P, bool root) {
    auto dsep = send - sbegin;
    auto dupd = upd.size();
    std::unique_ptr<FrontalMatrixMPI<scalar_t,integer_t>> front;
    switch (opts.compression()) {
    case CompressionType::HSS: {
      if (is_HSS(dsep, dupd, opts)) {
        front = std::make_unique<FrontHSSMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P);
        if (root) fc.HSS++;
      }
    } break;
    case CompressionType::BLR: {
      if (is_BLR(dsep, dupd, opts)) {
        front = std::make_unique<FrontBLRMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P, opts.BLR_options().leaf_size());
        if (root) fc.BLR++;
      }
    } break;
    case CompressionType::HODLR: {
#if defined(STRUMPACK_USE_BPACK)
      if (is_HODLR(dsep, dupd, opts)) {
        front = std::make_unique<FrontHODLRMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P);
        if (root) fc.HODLR++;
      }
#endif
    } break;
    case CompressionType::BLR_HODLR: {
#if defined(STRUMPACK_USE_BPACK)
      if (is_HODLR(dsep, dupd, opts, 0)) {
        front = std::make_unique<FrontHODLRMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P);
        if (root) fc.HODLR++;
      }
#endif
      if (!front && is_BLR(dsep, dupd, opts, 1)) {
        front = std::make_unique<FrontBLRMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P, opts.BLR_options().leaf_size());
        if (root) fc.BLR++;
      }
    } break;
    case CompressionType::ZFP_BLR_HODLR: {
#if defined(STRUMPACK_USE_BPACK)
      if (is_HODLR(dsep, dupd, opts, 0)) {
        front = std::make_unique<FrontHODLRMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P);
        if (root) fc.HODLR++;
      }
#endif
      if (!front && is_BLR(dsep, dupd, opts, 1)) {
        front = std::make_unique<FrontBLRMPI<scalar_t,integer_t>>
          (s, sbegin, send, upd, comm, P, opts.BLR_options().leaf_size());
        if (root) fc.BLR++;
      }
    } break;
    case CompressionType::H2: // not supported, add warning?
    case CompressionType::LOSSY: // handled in DenseMPI
    case CompressionType::LOSSLESS: // handled in DenseMPI
    case CompressionType::NONE: break;
    };
    // (NONE, LOSSLESS, LOSSY or not compiled with HODLR)
    if (!front) {
      front = std::make_unique<FrontalMatrixDenseMPI<scalar_t,integer_t>>
        (s, sbegin, send, upd, comm, P);
      if (root) fc.dense++;
    }
    return front;
  }

  template std::unique_ptr<FrontalMatrixMPI<float,int>>
  create_frontal_matrix(const SPOptions<float>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc,
                        const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<double,int>>
  create_frontal_matrix(const SPOptions<double>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc,
                        const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<std::complex<float>,int>>
  create_frontal_matrix(const SPOptions<std::complex<float>>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc,
                        const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<std::complex<double>,int>>
  create_frontal_matrix(const SPOptions<std::complex<double>>& opts, int s, int sbegin, int send,
                        std::vector<int>& upd, int level, FrontCounter& fc,
                        const MPIComm& comm, int P, bool root);

  template std::unique_ptr<FrontalMatrixMPI<float,long int>>
  create_frontal_matrix(const SPOptions<float>& opts, long int s, long int sbegin,
                        long int send, std::vector<long int>& upd, int level,
                        FrontCounter& fc, const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<double,long int>>
  create_frontal_matrix(const SPOptions<double>& opts, long int s, long int sbegin,
                        long int send, std::vector<long int>& upd, int level,
                        FrontCounter& fc, const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<std::complex<float>,long int>>
  create_frontal_matrix(const SPOptions<std::complex<float>>& opts, long int s,
                        long int sbegin, long int send, std::vector<long int>& upd,
                        int level, FrontCounter& fc, const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<std::complex<double>,long int>>
  create_frontal_matrix(const SPOptions<std::complex<double>>& opts, long int s,
                        long int sbegin, long int send, std::vector<long int>& upd,
                        int level, FrontCounter& fc, const MPIComm& comm, int P, bool root);

  template std::unique_ptr<FrontalMatrixMPI<float,long long int>>
  create_frontal_matrix(const SPOptions<float>& opts, long long int s, long long int sbegin,
                        long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<double,long long int>>
  create_frontal_matrix(const SPOptions<double>& opts, long long int s, long long int sbegin,
                        long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<std::complex<float>,long long int>>
  create_frontal_matrix(const SPOptions<std::complex<float>>& opts, long long int s,
                        long long int sbegin, long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, const MPIComm& comm, int P, bool root);
  template std::unique_ptr<FrontalMatrixMPI<std::complex<double>,long long int>>
  create_frontal_matrix(const SPOptions<std::complex<double>>& opts, long long int s,
                        long long int sbegin, long long int send, std::vector<long long int>& upd,
                        int level, FrontCounter& fc, const MPIComm& comm, int P, bool root);

#endif

} // end namespace strumpack
