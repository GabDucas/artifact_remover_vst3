// RingBufferExample.cpp

#include "artifact_remover/utils/CircularBuffer.h"

#include <iostream>

int main() {

    artifact_remover::utils::CircularBuffer buffer(2, 8);

    std::vector<std::vector<double>> data = {
        {1,2,3},
        {4,5,6}
    };

    std::vector<double> t = {0.0, 0.1, 0.2};

    buffer.append(data, t);

    auto [x, time] = buffer.get();

    for (size_t ch = 0; ch < x.size(); ++ch) {

        std::cout << "Channel " << ch << ": ";

        for (double v : x[ch]) {
            std::cout << v << " ";
        }

        std::cout << std::endl;
    }

    return 0;
}