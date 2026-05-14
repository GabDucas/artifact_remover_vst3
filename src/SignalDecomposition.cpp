#include "artifact_remover/core/SignalDecomposition.h"
#include <Eigen/SVD>
#include <vector>
#include <stdexcept>

namespace artifact_remover::core
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
    SVDResult compute_svd(const Vector &emg_signal, int n_rows, int hankel_delay)
    {
        SVDResult out;

        out.hankel = hankel_delay_fct(emg_signal, n_rows, hankel_delay);

        // Eigen::JacobiSVD<Matrix> svd(out.hankel, Eigen::ComputeThinU | Eigen::ComputeThinV);
        Eigen::BDCSVD<Matrix> svd(out.hankel, Eigen::ComputeThinU | Eigen::ComputeThinV);


        out.U = svd.matrixU();
        out.S = svd.singularValues();
        out.Vh = svd.matrixV().transpose();

        return out;
    }

} // namespace artifact_remover::core