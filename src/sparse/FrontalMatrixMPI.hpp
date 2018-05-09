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
 * five (5) year renewals, the U.S. Government igs granted for itself
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
#ifndef FRONTAL_MATRIX_MPI_HPP
#define FRONTAL_MATRIX_MPI_HPP

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include "misc/MPIWrapper.hpp"
#include "misc/TaskTimer.hpp"
#include "dense/DistributedMatrix.hpp"
#include "CompressedSparseMatrix.hpp"
#include "MatrixReordering.hpp"
#include "ExtendAdd.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t>
  class FrontalMatrixMPI : public FrontalMatrix<scalar_t,integer_t> {
    using SpMat_t = CompressedSparseMatrix<scalar_t,integer_t>;
    using FMPI_t = FrontalMatrixMPI<scalar_t,integer_t>;
    using F_t = FrontalMatrix<scalar_t,integer_t>;
    using DenseM_t = DenseMatrix<scalar_t>;
    using DistM_t = DistributedMatrix<scalar_t>;
    using DistMW_t = DistributedMatrixWrapper<scalar_t>;
    using ExtAdd = ExtendAdd<scalar_t,integer_t>;
    template<typename _scalar_t,typename _integer_t> friend class ExtendAdd;

  public:
    FrontalMatrixMPI
    (integer_t _sep, integer_t _sep_begin, integer_t _sep_end,
     std::vector<integer_t>& _upd, MPI_Comm _front_comm, int _total_procs);
    FrontalMatrixMPI(const FrontalMatrixMPI&) = delete;
    FrontalMatrixMPI& operator=(FrontalMatrixMPI const&) = delete;
    virtual ~FrontalMatrixMPI();

    void sample_CB
    (const SPOptions<scalar_t>& opts, const DenseM_t& R, DenseM_t& Sr,
     DenseM_t& Sc, F_t* parent, int task_depth=0) override {};
    virtual void sample_CB
    (const SPOptions<scalar_t>& opts, const DistM_t& R, DistM_t& Sr,
     DistM_t& Sc, F_t* pa) const = 0;

    void extract_2d
    (const SpMat_t& A, const std::vector<std::size_t>& I,
     const std::vector<std::size_t>& J, DistM_t& B) const;
    void extract_2d
    (const SpMat_t& A,
     const std::vector<std::vector<std::size_t>>& I,
     const std::vector<std::vector<std::size_t>>& J,
     std::vector<DistMW_t>& B) const;
    void get_submatrix_2d
    (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
     DistM_t& Bdist, DenseM_t& Bseq) const override;
    void get_submatrix_2d
    (const std::vector<std::vector<std::size_t>>& I,
     const std::vector<std::vector<std::size_t>>& J,
     std::vector<DistM_t>& Bdist, std::vector<DenseM_t>& Bseq) const override;
    void extract_CB_sub_matrix
    (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
     DenseM_t& B, int task_depth) const {};
    virtual void extract_CB_sub_matrix_2d
    (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
     DistM_t& B) const = 0;
    virtual void extract_CB_sub_matrix_2d
    (const std::vector<std::vector<std::size_t>>& I,
     const std::vector<std::vector<std::size_t>>& J,
     std::vector<DistM_t>& B) const;

    void extend_add_b
    (DistM_t& b, DistM_t& bupd, const DistM_t& CBl, const DistM_t& CBr,
     const DenseM_t& seqCBl, const DenseM_t& seqCBr) const;
    void extract_b
    (const DistM_t& b, const DistM_t& bupd, DistM_t& CBl, DistM_t& CBr,
     DenseM_t& seqCBl, DenseM_t& seqCBr) const;

    inline bool visit(const F_t* ch) const;
    inline int child_master(const F_t* ch) const;
    inline int blacs_context() const { return ctxt; }
    inline int blacs_context_all() const { return ctxt_all; }
    inline bool active() const {
      return front_comm != MPI_COMM_NULL &&
        mpi_rank(front_comm) < proc_rows*proc_cols;
    }
    static inline void processor_grid
    (int np_procs, int& np_rows, int& np_cols) {
      np_cols = std::floor(std::sqrt((float)np_procs));
      np_rows = np_procs / np_cols;
    }
    inline int np_rows() const { return proc_rows; }
    inline int np_cols() const { return proc_cols; }
    virtual long long dense_factor_nonzeros(int task_depth=0) const;
    virtual std::string type() const { return "FrontalMatrixMPI"; }
    virtual bool isMPI() const { return true; }
    MPI_Comm comm() const { return front_comm; }
    virtual void bisection_partitioning
    (const SPOptions<scalar_t>& opts, integer_t* sorder,
     bool isroot=true, int task_depth=0);

    friend class FrontalMatrixDenseMPI<scalar_t,integer_t>;
    friend class FrontalMatrixHSSMPI<scalar_t,integer_t>;

  protected:
    // this is a blacs context with only the active process for this front
    int ctxt;
    // this is a blacs context with all process for this front
    int ctxt_all;
    // number of process rows in the blacs ctxt
    int proc_rows;
    // number of process columns in the blacs ctxt
    int proc_cols;
    // number of processes that work on the subtree belonging to this front,
    int total_procs;
    // this can be more than the processes in the blacs context ctxt
    // and is not necessarily the same as mpi_nprocs(front_comm),
    // because if this rank is not part of front_comm,
    // mpi_nprocs(front_comm) == 0
    MPI_Comm front_comm;  // MPI communicator for this front
  };

  template<typename scalar_t,typename integer_t>
  FrontalMatrixMPI<scalar_t,integer_t>::FrontalMatrixMPI
  (integer_t _sep, integer_t _sep_begin, integer_t _sep_end,
   std::vector<integer_t>& _upd, MPI_Comm _front_comm, int _total_procs)
    : F_t(NULL, NULL,_sep, _sep_begin, _sep_end, _upd),
    total_procs(_total_procs), front_comm(_front_comm) {
    processor_grid(total_procs, proc_rows, proc_cols);
    if (front_comm != MPI_COMM_NULL) {
      int active_procs = proc_rows * proc_cols;
      if (active_procs < total_procs) {
        auto active_front_comm = mpi_sub_comm(front_comm, 0, active_procs);
        if (mpi_rank(front_comm) < active_procs) {
          ctxt = scalapack::Csys2blacs_handle(active_front_comm);
          scalapack::Cblacs_gridinit(&ctxt, "C", proc_rows, proc_cols);
        } else ctxt = -1;
        mpi_free_comm(&active_front_comm);
      } else {
        ctxt = scalapack::Csys2blacs_handle(front_comm);
        scalapack::Cblacs_gridinit(&ctxt, "C", proc_rows, proc_cols);
      }
      ctxt_all = scalapack::Csys2blacs_handle(front_comm);
      scalapack::Cblacs_gridinit(&ctxt_all, "R", 1, total_procs);
    } else ctxt = ctxt_all = -1;
  }

  template<typename scalar_t,typename integer_t>
  FrontalMatrixMPI<scalar_t,integer_t>::~FrontalMatrixMPI() {
    if (ctxt != -1) scalapack::Cblacs_gridexit(ctxt);
    if (ctxt_all != -1) scalapack::Cblacs_gridexit(ctxt_all);
    mpi_free_comm(&front_comm);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_2d
  (const SpMat_t& A, const std::vector<std::size_t>& I,
   const std::vector<std::size_t>& J, DistM_t& B) const {
    auto m = I.size();
    auto n = J.size();
    TIMER_TIME(TaskType::EXTRACT_SEP_2D, 2, t_ex_sep);
    {
      DistM_t tmp(ctxt, m, n);
      A.extract_separator_2d(this->sep_end, I, J, tmp, front_comm);
      // TODO why this copy???
      strumpack::copy(m, n, tmp, 0, 0, B, 0, 0, ctxt_all);
    }
    TIMER_STOP(t_ex_sep);
    TIMER_TIME(TaskType::GET_SUBMATRIX_2D, 2, t_getsub);
    DistM_t Bl, Br;
    DenseM_t Blseq, Brseq;
    if (visit(this->lchild)) this->lchild->get_submatrix_2d(I, J, Bl, Blseq);
    if (visit(this->rchild)) this->rchild->get_submatrix_2d(I, J, Br, Brseq);
    DistM_t tmp(B.ctxt(), m, n);
    if (this->lchild) {
      if (this->lchild->isMPI())
        strumpack::copy(m, n, Bl, 0, 0, tmp, 0, 0, ctxt_all);
      else
        strumpack::copy
          (m, n, Blseq, child_master(this->lchild),
           tmp, 0, 0, ctxt_all);
    }
    B.add(tmp);
    if (this->rchild) {
      if (this->rchild->isMPI())
        strumpack::copy(m, n, Br, 0, 0, tmp, 0, 0, ctxt_all);
      else
        strumpack::copy
          (m, n, Brseq, child_master(this->rchild),
           tmp, 0, 0, ctxt_all);
    }
    B.add(tmp);
    TIMER_STOP(t_getsub);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_2d
  (const SpMat_t& A,
   const std::vector<std::vector<std::size_t>>& I,
   const std::vector<std::vector<std::size_t>>& J,
   std::vector<DistMW_t>& B) const {
    TIMER_TIME(TaskType::EXTRACT_SEP_2D, 2, t_ex_sep);
    for (std::size_t i=0; i<I.size(); i++) {
      DistM_t tmp(ctxt, I[i].size(), J[i].size());
      A.extract_separator_2d(this->sep_end, I[i], J[i], tmp, front_comm);
      // TODO why this copy???
      strumpack::copy(I[i].size(), J[i].size(), tmp, 0, 0, B[i], 0, 0, ctxt_all);
    }
    TIMER_STOP(t_ex_sep);
    TIMER_TIME(TaskType::GET_SUBMATRIX_2D, 2, t_getsub);
    std::vector<DistM_t> Bl, Br;
    std::vector<DenseM_t> Blseq, Brseq;
    if (visit(this->lchild)) this->lchild->get_submatrix_2d(I, J, Bl, Blseq);
    if (visit(this->rchild)) this->rchild->get_submatrix_2d(I, J, Br, Brseq);

    DistM_t d;
    DenseM_t d2;
    for (std::size_t i=0; i<I.size(); i++) {
      auto m = I[i].size();
      auto n = J[i].size();
      if (!m || !n) continue;
      DistM_t tmp(B[i].ctxt(), m, n);
      // TODO combine all these copies???!!
      if (this->lchild) {
        if (this->lchild->isMPI())
          strumpack::copy
            (m, n, visit(this->lchild) ? Bl[i] : d, 0, 0,
             tmp, 0, 0, ctxt_all);
        else
          strumpack::copy
            (m, n, visit(this->lchild) ? Blseq[i] : d2,
             child_master(this->lchild), tmp, 0, 0, ctxt_all);
      }
      B[i].add(tmp);
      if (this->rchild) {
        if (this->rchild->isMPI())
          strumpack::copy
            (m, n, visit(this->rchild) ? Br[i] : d, 0, 0,
             tmp, 0, 0, ctxt_all);
        else
          strumpack::copy
            (m, n, visit(this->rchild) ? Brseq[i] : d2,
             child_master(this->rchild), tmp, 0, 0, ctxt_all);
      }
      B[i].add(tmp);
    }
    TIMER_STOP(t_getsub);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::get_submatrix_2d
  (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
   DistM_t& Bdist, DenseM_t&) const {
    Bdist = DistM_t(ctxt, I.size(), J.size());
    Bdist.zero();
    extract_CB_sub_matrix_2d(I, J, Bdist);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::get_submatrix_2d
  (const std::vector<std::vector<std::size_t>>& I,
   const std::vector<std::vector<std::size_t>>& J,
   std::vector<DistM_t>& Bdist, std::vector<DenseM_t>&) const {
    // TODO add timer here???
    for (std::size_t i=0; i<I.size(); i++) {
      Bdist.emplace_back(ctxt, I[i].size(), J[i].size());
      Bdist.back().zero();
    }
    extract_CB_sub_matrix_2d(I, J, Bdist);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_CB_sub_matrix_2d
  (const std::vector<std::vector<std::size_t>>& I,
   const std::vector<std::vector<std::size_t>>& J,
   std::vector<DistM_t>& B) const {
    for (std::size_t i=0; i<I.size(); i++)
      extract_CB_sub_matrix_2d(I[i], J[i], B[i]);
  }

  /**
   * Check if the child needs to be visited. Not necessary when this
   * rank is not part of the processes assigned to the child.
   */
  template<typename scalar_t,typename integer_t> bool
  FrontalMatrixMPI<scalar_t,integer_t>::visit(const F_t* ch) const {
    if (!ch) return false;
    if (auto mpi_child = dynamic_cast<const FMPI_t*>(ch)) {
      if (mpi_child->front_comm == MPI_COMM_NULL)
        return false; // child is MPI
    } else if (mpi_rank(front_comm) != child_master(ch))
      return false; // child is sequential
    return true;
  }

  template<typename scalar_t,typename integer_t> int
  FrontalMatrixMPI<scalar_t,integer_t>::child_master(const F_t* ch) const {
    int ch_master;
    if (auto mpi_ch = dynamic_cast<const FMPI_t*>(ch))
      ch_master = (ch == this->lchild) ? 0 :
        total_procs - mpi_ch->total_procs;
    else ch_master = (ch == this->lchild) ? 0 : total_procs - 1;
    return ch_master;
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extend_add_b
  (DistM_t& b, DistM_t& bupd, const DistM_t& CBl, const DistM_t& CBr,
   const DenseM_t& seqCBl, const DenseM_t& seqCBr) const {
    if (mpi_rank(front_comm) == 0) {
      STRUMPACK_FLOPS
        (static_cast<long long int>(CBl.rows()*b.cols()));
      STRUMPACK_FLOPS
        (static_cast<long long int>(CBr.rows()*b.cols()));
    }
    auto P = mpi_nprocs(this->front_comm);
    std::vector<std::vector<scalar_t>> sbuf(P);
    if (visit(this->lchild)) {
      if (this->lchild->isMPI())
        ExtAdd::extend_add_column_copy_to_buffers
          (CBl, sbuf, this, this->lchild->upd_to_parent(this));
      else ExtAdd::extend_add_column_seq_copy_to_buffers
             (seqCBl, sbuf, this, this->lchild);
    }
    if (visit(this->rchild)) {
      if (this->rchild->isMPI())
        ExtAdd::extend_add_column_copy_to_buffers
          (CBr, sbuf, this, this->rchild->upd_to_parent(this));
      else ExtAdd::extend_add_column_seq_copy_to_buffers
             (seqCBr, sbuf, this, this->rchild);
    }
    scalar_t *rbuf = nullptr, **pbuf = nullptr;
    all_to_all_v(sbuf, rbuf, pbuf, this->front_comm);
    for (auto ch : {this->lchild, this->rchild}) {
      if (!ch) continue;
      if (auto ch_mpi = dynamic_cast<FMPI_t*>(ch))
        ExtAdd::extend_add_column_copy_from_buffers
          (b, bupd, pbuf+this->child_master(ch), this, ch_mpi);
      else ExtAdd::extend_add_column_seq_copy_from_buffers
             (b, bupd, pbuf[this->child_master(ch)], this, ch);
    }
    delete[] pbuf;
    delete[] rbuf;
  }


  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_b
  (const DistM_t& b, const DistM_t& bupd, DistM_t& CBl, DistM_t& CBr,
   DenseM_t& seqCBl, DenseM_t& seqCBr) const {
    auto P = mpi_nprocs(this->front_comm);
    std::vector<std::vector<scalar_t>> sbuf(P);
    for (auto ch : {this->lchild, this->rchild}) {
      if (!ch) continue;
      if (auto ch_mpi = dynamic_cast<FMPI_t*>(ch))
        ExtAdd::extract_column_copy_to_buffers(b, bupd, sbuf, this, ch_mpi);
      else ExtAdd::extract_column_seq_copy_to_buffers
             (b, bupd, sbuf[this->child_master(ch)], this, ch);
    }
    scalar_t *rbuf = nullptr, **pbuf = nullptr;
    all_to_all_v(sbuf, rbuf, pbuf, this->front_comm);
    if (visit(this->lchild)) {
      if (auto ch_mpi = dynamic_cast<FMPI_t*>(this->lchild)) {
        CBl = DistM_t
          (ch_mpi->ctxt, this->lchild->dim_upd(), b.cols());
        ExtAdd::extract_column_copy_from_buffers
          (CBl, pbuf, this, this->lchild);
      } else {
        seqCBl = DenseM_t(this->lchild->dim_upd(), b.cols());
        ExtAdd::extract_column_seq_copy_from_buffers
          (seqCBl, pbuf, this, this->lchild);
      }
    }
    if (visit(this->rchild)) {
      if (auto ch_mpi = dynamic_cast<FMPI_t*>(this->rchild)) {
        CBr = DistM_t
          (ch_mpi->ctxt, this->rchild->dim_upd(), b.cols());
        ExtAdd::extract_column_copy_from_buffers(CBr, pbuf, this, this->rchild);
      } else {
        seqCBr = DenseM_t(this->rchild->dim_upd(), b.cols());
        ExtAdd::extract_column_seq_copy_from_buffers
          (seqCBr, pbuf, this, this->rchild);
      }
    }
    delete[] pbuf;
    delete[] rbuf;
  }

  template<typename scalar_t,typename integer_t> long long
  FrontalMatrixMPI<scalar_t,integer_t>::dense_factor_nonzeros
  (int task_depth) const {
    long long nnz = 0;
    if (this->front_comm != MPI_COMM_NULL &&
        mpi_rank(this->front_comm) == 0) {
      long long dsep = this->dim_sep();
      long long dupd = this->dim_upd();
      nnz = dsep * (dsep + 2 * dupd);
    }
    if (visit(this->lchild))
      nnz += this->lchild->dense_factor_nonzeros(task_depth);
    if (visit(this->rchild))
      nnz += this->rchild->dense_factor_nonzeros(task_depth);
    return nnz;
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::bisection_partitioning
  (const SPOptions<scalar_t>& opts, integer_t* sorder,
   bool isroot, int task_depth) {
    for (integer_t i=this->sep_begin; i<this->sep_end; i++) sorder[i] = -i;

    if (visit(this->lchild))
      this->lchild->bisection_partitioning(opts, sorder, false, task_depth);
    if (visit(this->rchild))
      this->rchild->bisection_partitioning(opts, sorder, false, task_depth);
  }

} // end namespace strumpack

#endif //FRONTAL_MATRIX_MPI_HPP
