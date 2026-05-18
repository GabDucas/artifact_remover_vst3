// FFT.cpp
// #define EIGEN_POCKETFFT_DEFAULT
// #define EIGEN_FFTW_DEFAULT


#include "artifact_remover/FFT.h"
#include <vector>
#include <iostream>

namespace artifact_remover
{

ComplexMatrix FFT::rfft(const Matrix& x)
{
    const int nch = x.rows();
    const int nsamples = x.cols();

    const int nfreq = nsamples / 2 + 1;


    ComplexMatrix out(nch, nfreq);

    std::vector<std::complex<double>> freq(nsamples);

    for (int ch = 0; ch < nch; ++ch)
    {
        Eigen::VectorXd signal = x.row(ch).transpose().eval();
        // std::cout << x.row(ch).innerStride() << std::endl;

        fft_.fwd(freq.data(), signal.data(), nsamples);

        for (int k = 0; k < nfreq; ++k)
        {
            out(ch, k) = freq[k];
        }
    }

    return out;
}


Matrix FFT::rfft_mag(const Matrix& x)
{
    const int nch = x.rows();
    const int nsamples = x.cols();

    const int nfreq = nsamples / 2 + 1;

    
    Matrix out(nch, nfreq);

    std::vector<std::complex<double>> freq(nsamples);

    for (int ch = 0; ch < nch; ++ch)
    {
        Eigen::VectorXd signal = x.row(ch).transpose().eval();
        // std::cout << x.row(ch).innerStride() << std::endl;

        fft_.fwd(freq.data(), signal.data(), nsamples);

        for (int k = 0; k < nfreq; ++k)
        {
            out(ch, k) = std::abs(freq[k]);
        }
    }

    return out;
}

std::vector<double> FFT::get_fft_frequencies(int N, double Fs)  
{
    // For real-input FFT, the number of unique bins is N/2 + 1
    int numBins = N / 2 + 1;
    std::vector<double> frequencies(numBins);
    
    double resolution = Fs / N;

    for (int k = 0; k < numBins; ++k) {
        frequencies[k] = k * resolution;
    }
    
    return frequencies;
}

} // namespace artifact_remover