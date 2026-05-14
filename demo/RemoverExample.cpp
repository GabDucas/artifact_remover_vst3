// =========================================================
// Example.cpp
// =========================================================
#include <chrono>
#include <iostream>
#include <fstream>
#include <cmath>

#include "artifact_remover/dsp/Remover.h"

using namespace artifact_remover::dsp;
using namespace artifact_remover::core;

int main()
{
    // =====================================================
    // 1. Load signal from CSV
    // =====================================================

    const double fs = 1925.9;

    std::ifstream file("D:\\Documents\\Programmation\\tscs_artifact_remover\\signal.txt");

    std::vector<double> temp;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty()) {
            temp.push_back(std::stod(line));
        }
    }

    Eigen::VectorXd signal(temp.size());

    for (size_t i = 0; i < temp.size(); ++i) {
        signal(i) = temp[i];
    }

    std::cout << "Signal loaded\n";

    // =====================================================
    // 2. Create remover
    // =====================================================

    Remover remover;

    // =====================================================
    // 3. Run pipeline
    // =====================================================

    int L = 100;
    int tau = 1;
    // get only the 10 000 first samples for faster testing
    std::cout << signal.size() << " samples\n";
    auto result =
        remover.remove_artifact(
            signal.head(500),
            L,
            tau,
            fs,
            10.0,   // lower frequency bound
            450.0,  // upper frequency bound
            0.3     // rejection factor
        );

    std::cout << "Artifact removal completed\n";

    // =====================================================
    // 4. Display rejected components
    // =====================================================

    std::cout << "\nRejected singular vectors:\n";

    for (int idx : result.rejected_idx)
    {
        std::cout << idx << "\n";
    }

    // =====================================================
    // 5. Display first singular values
    // =====================================================

    std::cout << "\nTop singular values:\n";

    for (
        int i = 0;
        i < std::min(10, (int)result.S.size());
        ++i
    )
    {
        std::cout
            << "S[" << i << "] = "
            << result.S(i)
            << "\n";
    }

    // =====================================================
    // 6. Reconstruction quality
    // =====================================================

    double original_energy =
        signal.squaredNorm();

    double cleaned_energy =
        result.cleaned_signal.squaredNorm();

    std::cout
        << "\nOriginal energy: "
        << original_energy
        << "\n";

    std::cout
        << "Cleaned energy: "
        << cleaned_energy
        << "\n";

    // =====================================================
    // 7. Export CSV
    // =====================================================

    std::ofstream out("cleaned_signal.csv");

    out << "original,cleaned\n";

    for (int i = 0; i < signal.size(); ++i)
    {
        out
            << signal(i)
            << ","
            << result.cleaned_signal(i)
            << "\n";
    }

    out.close();

    std::cout
        << "\nCSV exported: cleaned_signal.csv\n";

    return 0;
}