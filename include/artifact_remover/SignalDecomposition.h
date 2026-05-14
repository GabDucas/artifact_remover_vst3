
#include <Eigen/Dense>
#include <tuple>

namespace artifact_remover::core {

using Matrix = Eigen::Matrix<
        double,
        Eigen::Dynamic,
        Eigen::Dynamic,
        Eigen::RowMajor>;

using Vector = Eigen::VectorXd;

struct SVDResult
{
    Matrix U;
    Vector S;
    Matrix Vh;
    Matrix hankel;
};

/**
 * Construct a delayed Hankel matrix from a 1D signal.
 */
Matrix hankel_delay_fct(const Vector& x, int L, int tau);

/**
 * Reconstruct a 1D signal from a Hankel matrix via anti-diagonal averaging.
 */
Vector reconstruct_from_hankel(const Matrix& hankel, int tau = 1);

/**
 * Compute SVD of a Hankel-embedded signal.
 */
SVDResult compute_svd(const Vector& emg_signal, int n_rows = 800, int hankel_delay = 1);

} // namespace artifact_remover::core