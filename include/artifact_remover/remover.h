// =========================================================
// Remover.h
// =========================================================

#include <Eigen/Dense>
#include <complex>
#include <vector>

#include "artifact_remover/core/SignalDecomposition.h"
#include "artifact_remover/core/FFT.h"

namespace artifact_remover::dsp
{

using Matrix = Eigen::Matrix<
    double,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::RowMajor>;

using Vector = Eigen::VectorXd;

using ComplexMatrix = Eigen::Matrix<
    std::complex<double>,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::RowMajor>;

struct RemovalResult
{
    Vector cleaned_signal;

    Matrix hankel;

    Matrix U;
    Vector S;
    Matrix Vh;

    std::vector<int> rejected_idx;
};

class Remover
{
public:

    Remover() = default;

    RemovalResult remove_artifact(
        const Vector& signal,
        int L,
        int tau,
        double data_rate,
        double lower_freq = 10.0,
        double upper_freq = 450.0,
        double factor = 0.5
    );

private:

    artifact_remover::core::FFT fft_;

    // -----------------------------------------------------
    // Metrics
    // -----------------------------------------------------

    Vector mean_around_row_max(
        const Matrix& X,
        int w
    ) const;

    Vector peak_energy_ratio(
        const Matrix& X
    ) const;

    // -----------------------------------------------------
    // Component rejection
    // -----------------------------------------------------

    std::vector<int> find_components_to_reject(
        const Matrix& Vh,
        double data_rate,
        double lower_freq,
        double upper_freq,
        double factor
    );

    // -----------------------------------------------------
    // Reconstruction
    // -----------------------------------------------------

    Matrix reconstruct_hankel(
        const Matrix& U,
        const Vector& S,
        const Matrix& Vh
    ) const;
};

}