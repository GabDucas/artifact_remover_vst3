
// #define EIGEN_USE_MKL_ALL

#include "artifact_remover/SignalDecomposition.h"
#include <Eigen/SVD>
#include <vector>
#include <stdexcept>
#include <iostream>

namespace artifact_remover
{

    /**
     * \brief Build delayed Hankel matrix: H(i, j) = x(i*tau + j)
     * \param x Input signal (1D)
     * \param L Number of rows (embedding dimension)
     * \param tau Delay between rows
     *
     */
    Matrix hankel_delay_fct(const Vector &x, int L, int tau)
    {
        const int N = static_cast<int>(x.size());
        const int K = N - (L - 1) * tau;

        if (K <= 0)
            throw std::runtime_error("L and tau too large for signal length");

        Matrix H(L, K);

        for (int i = 0; i < L; ++i)
        {
            const int offset = i * tau;
            for (int j = 0; j < K; ++j)
            {
                H(i, j) = x[offset + j];
            }
        }

        return H;
    }

    /**
     * \brief Anti-diagonal reconstruction (equivalent to bincount averaging).
     * \param H Hankel matrix
     * \param tau Delay between rows (default 1 for standard Hankel)
     */
    Vector reconstruct_from_hankel(const Matrix &H, int tau)
    {
        const int rows = H.rows();
        const int cols = H.cols();

        const int total_length = (rows - 1) * tau + cols;

        Vector sums = Vector::Zero(total_length);
        Vector counts = Vector::Zero(total_length);

        for (int i = 0; i < rows; ++i)
        {
            const int base = i * tau;
            for (int j = 0; j < cols; ++j)
            {
                const int idx = base + j;
                sums(idx) += H(i, j);
                counts(idx) += 1.0;
            }
        }

        Vector result(total_length);

        for (int k = 0; k < total_length; ++k)
        {
            result(k) = (counts(k) > 0.0) ? (sums(k) / counts(k)) : 0.0;
        }

        return result;
    }

    /**
     \brief SVD of Hankel matrix using Eigen (thin SVD = SciPy equivalent full_matrices=False)
        \param emg_signal 1D EMG signal
        \param n_rows Number of rows in Hankel matrix (L)
        \param hankel_delay Delay between rows (tau)

     */
    SVDResult compute_svd(const Vector &emg_signal, int n_rows, int hankel_delay, double threshold)
    {
        SVDResult out;
        out.hankel = hankel_delay_fct(emg_signal, n_rows, hankel_delay);
        
        // Convert to column-major for MKL compatibility
        ColMajorMatrix hankel_colmajor = out.hankel;
        Eigen::BDCSVD<ColMajorMatrix> svd(hankel_colmajor, Eigen::ComputeThinU | Eigen::ComputeThinV);

        Matrix U_full = svd.matrixU();
        Vector S_full = svd.singularValues();
        Matrix Vh_full = svd.matrixV().transpose();

        // Apply truncation if threshold provided
        if (threshold > 0.0) {
            double max_s = S_full(0);
            double threshold_value = threshold;
            
            std::vector<int> kept_indices;
            for (int i = 0; i < S_full.size(); ++i) {
                if (S_full(i) > threshold_value) {
                    kept_indices.push_back(i);
                }
            }
            
            int rank = kept_indices.size();
            
            // Extract only kept components
            out.U = Matrix(U_full.rows(), rank);
            out.S = Vector(rank);
            out.Vh = Matrix(rank, Vh_full.cols());
            
            for (int i = 0; i < rank; ++i) {
                int idx = kept_indices[i];
                out.U.col(i) = U_full.col(idx);
                out.S(i) = S_full(idx);
                out.Vh.row(i) = Vh_full.row(idx);
            }
        } else {
            // No truncation
            out.U = U_full;
            out.S = S_full;
            out.Vh = Vh_full;
        }

        return out;
    }

} // namespace artifact_remover