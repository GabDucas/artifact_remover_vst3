
// #define EIGEN_USE_MKL_ALL

#include <Eigen/Dense>
#include <tuple>
#include "artifact_remover/Types.h"

namespace artifact_remover 
{

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
 * @param emg_signal Input signal
 * @param n_rows Number of rows in Hankel matrix
 * @param hankel_delay Delay between rows
 * @param threshold Optional: if > 0, truncate singular values below (threshold * max_singular_value)
 */
SVDResult compute_svd(const Vector& emg_signal, int n_rows = 800, int hankel_delay = 1, double threshold = 0.0);

} // namespace artifact_remover