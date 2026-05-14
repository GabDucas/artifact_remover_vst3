// FFT.h
// #define EIGEN_POCKETFFT_DEFAULT

#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>
#include <complex>

namespace artifact_remover::core
{

// using Matrix = Eigen::MatrixXd;
/*
Row-major storage order is required for the FFT to work correctly with the current implementation of Eigen's FFT module,
which expects contiguous data in memory. Using row-major storage ensures that the data is stored in a way that allows
 for efficient access patterns during the FFT computation.

 */  
using Matrix = Eigen::Matrix<
        double,
        Eigen::Dynamic,
        Eigen::Dynamic,
        Eigen::RowMajor>;
using ComplexMatrix = Eigen::Matrix<
        std::complex<double>,
        Eigen::Dynamic,
        Eigen::Dynamic,
        Eigen::RowMajor>;

class FFT
{
public:
    FFT() = default;


        /**
     * @brief Compute real FFT independently on each channel.
     *
     * Input matrix shape:
     *   nchannels x nsamples
     * magnitude: if true, return the magnitude of the FFT instead of the complex values. Default is false.
     *
     * Output matrix shape:
     *   nchannels x (nsamples / 2 + 1)
     */
    ComplexMatrix rfft(const Matrix& x);
    Matrix rfft_mag(const Matrix& x);


    /**
     * @brief Get the frequencies corresponding to the FFT bins.
     *
     * @param N Number of samples (input length to FFT)
     * @param Fs Sampling frequency
     * @return std::vector<double> Vector of frequencies for each FFT bin
     */
    std::vector<double> get_fft_frequencies(int N, double Fs);

private:
    Eigen::FFT<double> fft_;
};

} // namespace artifact_remover::core