// =========================================================
// Example.cpp
// =========================================================
#include <chrono>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdlib>

#include "artifact_remover/Remover.h"

using namespace artifact_remover;

int main()
{
    // _putenv_s("MKL_VERBOSE", "1");
    // // Set MKL to sequential mode before using MKL functions

    // =====================================================
    // 1. Load signal from CSV
    // =====================================================

    const double fs = 1925.9;

    std::ifstream file("C:\\Users\\neuromobility_lab\\Documents\\amedeo\\dev\\artifact_remover_vst3\\demo\\signal.txt");

    if (!file.is_open()) {
        std::cerr << "Error: Could not open signal file\n";
        return 1;
    }

    std::vector<double> temp;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty()) {
            temp.push_back(std::stod(line));
        }
    }

    file.close();

    if (temp.empty()) {
        std::cerr << "Error: Signal file is empty\n";
        return 1;
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
    std::cout.flush();
    
    std::cout << "Starting artifact removal...\n";
    std::cout.flush();
    auto start = std::chrono::high_resolution_clock::now();
    auto result =
        remover.remove_artifact(
            signal.head(500),
            L,
            tau,
            fs,
            10.0,   // lower frequency bound
            450.0,  // upper frequency bound
            0.3,    // rejection factor
            0.0005    // SVD truncation threshold (relative to max singular value)
        );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total execution time: " << duration.count() << " ms\n";

    std::cout << "Artifact removal completed\n";
    std::cout.flush();

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