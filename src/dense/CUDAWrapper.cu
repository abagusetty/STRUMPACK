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
#include "CUDAWrapper.hpp"

namespace strumpack {
  namespace gpu {

    template<typename scalar_t> __global__ void
    laswp_kernel(int n, scalar_t* dA, int lddA,
                 int npivots, int* dipiv, int inci) {
      int tid = threadIdx.x + blockDim.x*blockIdx.x;
      if (tid < n) {
        dA += tid * lddA;
        auto A1 = dA;
        for (int i1=0; i1<npivots; i1++) {
          int i2 = dipiv[i1*inci] - 1;
          auto A2 = dA + i2;
          auto temp = *A1;
          *A1 = *A2;
          *A2 = temp;
          A1++;
        }
      }
    }

    template<typename scalar_t> void
    laswp(BLASHandle& handle, DenseMatrix<scalar_t>& dA,
          int k1, int k2, int* dipiv, int inci) {
      if (!dA.rows() || !dA.cols()) return;
      int n = dA.cols(), nt = 256;
      int grid = (n + nt - 1) / nt;
      cudaStream_t streamId;
      cublasGetStream(handle, &streamId);
      laswp_kernel<scalar_t><<<grid, nt, 0, streamId>>>
        (n, dA.data(), dA.ld(), k2-k1+1, dipiv+k1-1, inci);
      gpu_check(cudaPeekAtLastError());
    }

    template<typename T>  __global__ void
    laswp_vbatch_kernel(int* dn, T** dA, int* lddA, int** dipiv,
                        int* npivots, unsigned int batchCount) {
      // assume dn = cols, inc = 1
      int x = blockIdx.x * blockDim.x + threadIdx.x,
        f = blockIdx.y * blockDim.y + threadIdx.y;
      if (f >= batchCount) return;
      if (x >= dn[f]) return;
      auto A = dA[f];
      auto P = dipiv[f];
      auto ldA = lddA[f];
      auto npiv = npivots[f];
      A += x * ldA;
      auto A1 = A;
      for (int i=0; i<npiv; i++) {
        auto p = P[i] - 1;
        if (p != i) {
          auto A2 = A + p;
          auto temp = *A1;
          *A1 = *A2;
          *A2 = temp;
        }
        A1++;
      }
    }

    template<typename scalar_t> void
    laswp_fwd_vbatched(BLASHandle& handle, int* dn, int max_n,
                       scalar_t** dA, int* lddA, int** dipiv, int* npivots,
                       unsigned int batchCount) {
      if (max_n <= 0 || !batchCount) return;
      unsigned int nt = 512, ops = 1;
      while (nt > max_n) {
        nt /= 2;
        ops *= 2;
      }
      ops = std::min(ops, batchCount);
      unsigned int nbx = (max_n + nt - 1) / nt,
        nbf = (batchCount + ops - 1) / ops;
      dim3 block(nt, ops), grid(nbx, nbf);
      // assume that nbf < MAX_BLOCKS_Z = 65535, should be ok
      cudaStream_t streamId;
      cublasGetStream(handle, &streamId);
      laswp_vbatch_kernel<<<grid, block, 0, streamId>>>
        (dn, dA, lddA, dipiv, npivots, batchCount);
      gpu_check(cudaPeekAtLastError());
    }

    // explicit template instantiations
    template void laswp(BLASHandle&, DenseMatrix<float>&, int, int, int*, int);
    template void laswp(BLASHandle&, DenseMatrix<double>&, int, int, int*, int);
    template void laswp(BLASHandle&, DenseMatrix<std::complex<float>>&, int, int, int*, int);
    template void laswp(BLASHandle&, DenseMatrix<std::complex<double>>&, int, int, int*, int);

    template void laswp_fwd_vbatched(BLASHandle&, int*, int, float**, int*, int**, int*, unsigned int);
    template void laswp_fwd_vbatched(BLASHandle&, int*, int, double**, int*, int**, int*, unsigned int);
    template void laswp_fwd_vbatched(BLASHandle&, int*, int, std::complex<float>**, int*, int**, int*, unsigned int);
    template void laswp_fwd_vbatched(BLASHandle&, int*, int, std::complex<double>**, int*, int**, int*, unsigned int);

  } // end namespace gpu
} // end namespace strumpack
