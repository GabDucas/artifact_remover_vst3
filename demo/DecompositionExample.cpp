#include <iostream>
#include <fstream>
#include <Eigen/Dense>
#include <vector>

#include "artifact_remover/core/SignalDecomposition.h"
#include "artifact_remover/core/FFT.h"

using namespace artifact_remover::core;

int main()
{
    // -----------------------------
    // 1. Create synthetic signal
    // -----------------------------
    const int N = 4096;
    Vector x(N);

    double f = 0.01; // low-frequency sinusoid

    for (int i = 0; i < N; ++i)
    {
        // clean sinusoid + small noise
        x(i) = std::sin(2.0 * M_PI * f * i);
            //  + 0.05 * ((double)std::rand() / RAND_MAX - 0.5);
    }

    std::cout << "Signal generated\n";

    // -----------------------------
    // 2. Build Hankel + SVD
    // -----------------------------
    int L = 100;
    int tau = 1;

    auto result = compute_svd(x, L, tau);

    std::cout << "SVD computed\n";
    std::cout << "U: " << result.U.rows() << " x " << result.U.cols() << "\n";
    std::cout << "S: " << result.S.size() << "\n";
    std::cout << "Vh: " << result.Vh.rows() << " x " << result.Vh.cols() << "\n";

    // -----------------------------
    // 3. Inspect singular values
    // -----------------------------
    std::cout << "\nTop singular values:\n";
    for (int i = 0; i < std::min(10, (int)result.S.size()); ++i)
    {
        std::cout << "S[" << i << "] = " << result.S(i) << "\n";
    }

    // get the number of singular values greater than 0 (1e-8)
    int nSingValue = 0;
    for (int i = 0; i < result.S.size(); ++i)
    {
        if (result.S(i) > 1e-8)
            nSingValue++;
    }

    std::cout << "Number of significant singular values: " << nSingValue << "\n";

    // apply FFT on Vh matrix
    FFT fft;
    ComplexMatrix Vh_fft = fft.rfft(result.Vh);
    std::vector<double> fft_freqs = fft.get_fft_frequencies(result.Vh.cols(), 2000.0); // Assuming a sampling rate of 1000 Hz

    // concatenate frequencies and magnitudes for export


    std::ofstream outFile("output.csv");

    if (!outFile.is_open())
    {
        std::cerr << "Error: Could not open file\n";
        return 1;
    }

    const int nch = Vh_fft.rows();
    const int nfreq = Vh_fft.cols();

    // Header
    outFile << "frequency";

    for (int ch = 0; ch < nch; ++ch)
    {
        outFile << ",ch" << ch << "_real"
                << ",ch" << ch << "_imag"
                << ",ch" << ch << "_mag";
    }

    outFile << "\n";

    // Data
    for (int k = 0; k < nfreq; ++k)
    {
        outFile << fft_freqs[k];

        for (int ch = 0; ch < nch; ++ch)
        {
            const auto& val = Vh_fft(ch, k);

            outFile << ","
                    << val.real()
                    << ","
                    << val.imag()
                    << ","
                    << std::abs(val);
        }

        outFile << "\n";
    }

    outFile.close();

    std::cout << "Successfully written to output.csv" << std::endl;

    std::cout << "FFT exported to fft_output.csv"
              << std::endl;

    std::cout << "FFT shape: "
            << Vh_fft.rows() << " x "
            << Vh_fft.cols()
            << "\n";
    double sampling_rate = 2000.0; // Hz

    for (int ch = 0; ch < Vh_fft.rows(); ++ch)
    {
    Eigen::Index max_idx;

    // Magnitude spectrum
    auto magnitudes =
        Vh_fft.row(ch).cwiseAbs();

    // Find max magnitude index
    double max_value =
        magnitudes.maxCoeff(&max_idx);

    // Convert FFT bin to frequency
    double freq =
        static_cast<double>(max_idx)
        * sampling_rate
        / Vh_fft.cols();

    std::cout << "Channel " << ch
              << " max frequency: "
              << freq << " Hz"
              << " (magnitude = "
              << max_value
              << ")"
              << std::endl;
    }
    // assert(nSingValue == 2); 

    // -----------------------------
    // 4. Reconstruct Hankel matrix
    // -----------------------------
    Matrix Sigma = Matrix::Zero(result.U.cols(), result.Vh.rows());
    Sigma.diagonal() = result.S;
    Matrix hank_rec = result.U * Sigma * result.Vh;

    Vector A_rec = reconstruct_from_hankel(hank_rec, tau);

    double rel_error =
        (x - A_rec).norm() / x.norm();

    std::cout << "\nReconstruction relative error: "
              << rel_error << std::endl;

    return 0;
}