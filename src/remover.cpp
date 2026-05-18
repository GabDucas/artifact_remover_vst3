// =========================================================
// Remover.cpp
// =========================================================

#include "artifact_remover/Remover.h"

#include <algorithm>
#include <numeric>
#include <fstream>
#include <cmath>
#include <iostream>
#include <set>

namespace artifact_remover
{

    // =========================================================
    // Mean around row maximum
    // =========================================================

    // ─────────────────────────────────────────────
    Vector Remover::mean_around_row_max(const Matrix& X, int w) const 
    {
        const int N = X.rows();
        const int M = X.cols();
        Vector result(N);

        for (int i = 0; i < N; ++i) {
            // Find argmax of row i
            int max_col = 0;
            X.row(i).maxCoeff(&max_col);

            // Window [start, end)
            int start = std::max(0, max_col - w);
            int end   = std::min(M, max_col + w + 1);

            result(i) = X.row(i).segment(start, end - start).mean();
        }
        return result;
    }
    // =========================================================
    // Peak energy ratio
    // =========================================================

    Vector Remover::peak_energy_ratio(const Matrix& X) const {
        const int M = X.cols();
        const int w = static_cast<int>(M * 5.0 / 100.0);

        // P = |X|^2  (X is real here, so just X^2)
        Matrix P = X.array().abs2();

        // Max of each row
        Vector row_max = P.rowwise().maxCoeff();

        // Mean around row max
        Vector denom = mean_around_row_max(P, w);

        return row_max.array() / denom.array();
    }


    // =========================================================
    // Find rejected singular vectors
    // =========================================================

    std::vector<int> Remover::find_components_to_reject(
        const Matrix &Vh,
        double data_rate,
        double lower_freq,
        double upper_freq,
        double factor)
    {
        Matrix fft_mag = fft_.rfft_mag(Vh);
        // Frequency at argmax of each row's FFT
        std::vector<double> freqs =
            fft_.get_fft_frequencies(
                Vh.cols(),
                data_rate);

        Vector peak_freqs(fft_mag.rows());
        for (int i = 0; i < fft_mag.rows(); ++i) {
            int max_k = 0;
            fft_mag.row(i).maxCoeff(&max_k);
            // std::cout << "Row " << i << ": max at bin " << max_k << " with value " << fft_mag(i, max_k) << "\n";
            peak_freqs(i) = freqs[max_k];
        }

        Vector peak_ratio = peak_energy_ratio(fft_mag);
        Vector ratio = peak_ratio / peak_ratio.maxCoeff();

        // Median and std of ratio
        std::vector<double> ratio_vec(ratio.data(),
                                      ratio.data() + ratio.size());
        std::sort(ratio_vec.begin(), ratio_vec.end());
        double med = ratio_vec.size() % 2 == 0
            ? 0.5 * (ratio_vec[ratio_vec.size()/2 - 1] +
                     ratio_vec[ratio_vec.size()/2])
            : ratio_vec[ratio_vec.size()/2];

        double mean_r = std::accumulate(ratio_vec.begin(),
                                        ratio_vec.end(), 0.0)
                        / ratio_vec.size();
        double var = 0.0;
        for (double r : ratio_vec) var += (r - mean_r) * (r - mean_r);
        double std_r = std::sqrt(var / ratio_vec.size());
        double threshold = med + factor * std_r;

        // right frequencies in a file 
        // std::ofstream file("freqs.txt");
        // for (const auto& item : freqs){
        //     file << item << "\n";
        // }
        // file.close();
        // right fft magnitudes in a file
        // std::ofstream file("fft_mag.txt");
        // for (int i = 0; i < fft_mag.rows(); ++i) {
        //     for (int j = 0; j < fft_mag.cols(); ++j) {
        //         file << fft_mag(i, j) << " ";
        //     }
        //     file << "\n";
        // }
        // file.close();

        // std::cout << "Threshold for rejection: " << threshold << "med"<< med << "std_r" << std_r << "mean_r" << mean_r << "\n";
        // std::cout << "Peak frequencies: ";
        // for (double f : peak_freqs) std::cout << f << " ";
        // std::cout << "\n";

        std::vector<int> rejected;

        // Collect indices to reject
        std::set<int> to_reject_set;
        for (int i = 0; i < static_cast<int>(fft_mag.rows()); ++i) {
            if (peak_freqs(i) <= lower_freq || peak_freqs(i) >= upper_freq)
                to_reject_set.insert(i);
            if (ratio(i) > threshold)
                to_reject_set.insert(i);
        }
        rejected.assign(to_reject_set.begin(), to_reject_set.end());
        return rejected;
    }
    

    // =========================================================
    // Reconstruct Hankel matrix
    // =========================================================

    Matrix Remover::reconstruct_hankel(
        const Matrix &U,
        const Vector &S,
        const Matrix &Vh) const
    {
        Matrix Sigma =
            Matrix::Zero(
                U.cols(),
                Vh.rows());

        Sigma.diagonal() = S;

        return U * Sigma * Vh;
    }

    // =========================================================
    // Main pipeline
    // =========================================================

    RemovalResult Remover::remove_artifact(
        const Vector &signal,
        int L,
        int tau,
        double data_rate,
        double lower_freq,
        double upper_freq,
        double factor,
        double svd_threshold)
    {
        RemovalResult result;

        // -----------------------------------------------------
        // 1. Compute Hankel + SVD
        // -----------------------------------------------------
        // TODO: send template Eigen matrix to compute_svd to avoid copying data

        auto svd_result =
            artifact_remover::compute_svd(signal, L, tau, svd_threshold);

        result.hankel = svd_result.hankel;

        result.U = svd_result.U;
        result.S = svd_result.S;
        result.Vh = svd_result.Vh;

        // -----------------------------------------------------
        // 2. Find singular vectors to reject
        // -----------------------------------------------------

        result.rejected_idx =
            find_components_to_reject(
                result.Vh,
                data_rate,
                lower_freq,
                upper_freq,
                factor);

        // -----------------------------------------------------
        // 3. Zero rejected singular values
        // -----------------------------------------------------

        for (int idx : result.rejected_idx)
        {
            result.S(idx) = 0.0;
        }

        // -----------------------------------------------------
        // 4. Reconstruct Hankel
        // -----------------------------------------------------

        Matrix H_clean =
            reconstruct_hankel(
                result.U,
                result.S,
                result.Vh);

        // -----------------------------------------------------
        // 5. Recover signal
        // -----------------------------------------------------

        result.cleaned_signal =
            artifact_remover::reconstruct_from_hankel(
                H_clean,
                tau);
        
        std::ofstream file("cleaned_signal.txt");
        for (const auto& item : result.cleaned_signal){
            file << item << "\n";
        }
        file.close();


        return result;
    }

}