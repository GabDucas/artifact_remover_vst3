// RingBuffer.cpp


#include "utils/RingBuffer.h"

namespace artifact_remover::utils
{
    RingBuffer::RingBuffer(
        size_t nChannels,
        size_t bufferSize
    )
        : nChannels_(nChannels),
        size_(bufferSize),
        buffer_(nChannels * bufferSize, 0.0f)
    {
    }

    void RingBuffer::append(
        const float* const* data,
        size_t nSamples
    )
    {
        if (nSamples > size_)
        {
            throw std::runtime_error(
                "Block size larger than ring buffer"
            );
        }

        size_t writePos =
            writePos_.load(std::memory_order_relaxed);

        size_t end = writePos + nSamples;

        // Wraparound
        if (end > size_)
        {
            size_t split = size_ - writePos;

            for (size_t ch = 0; ch < nChannels_; ++ch)
            {
                // tail
                std::copy(
                    data[ch],
                    data[ch] + split,
                    &sample(ch, writePos)
                );

                // head
                std::copy(
                    data[ch] + split,
                    data[ch] + nSamples,
                    &sample(ch, 0)
                );
            }
        }
        else
        {
            for (size_t ch = 0; ch < nChannels_; ++ch)
            {
                std::copy(
                    data[ch],
                    data[ch] + nSamples,
                    &sample(ch, writePos)
                );
            }
        }

        writePos =
            (writePos + nSamples) % size_;

        writePos_.store(
            writePos,
            std::memory_order_release
        );
    }

    void RingBuffer::getLatest(
        float* const* output,
        size_t nSamples
    ) const
    {
        if (nSamples > size_)
        {
            throw std::runtime_error(
                "Requested samples larger than buffer"
            );
        }

        size_t writePos =
            writePos_.load(std::memory_order_acquire);

        size_t start =
            (writePos + size_ - nSamples) % size_;

        // Wraparound
        if (start + nSamples > size_)
        {
            size_t split = size_ - start;

            for (size_t ch = 0; ch < nChannels_; ++ch)
            {
                // tail
                std::copy(
                    &sample(ch, start),
                    &sample(ch, size_),
                    output[ch]
                );

                // head
                std::copy(
                    &sample(ch, 0),
                    &sample(ch, nSamples - split),
                    output[ch] + split
                );
            }
        }
        else
        {
            for (size_t ch = 0; ch < nChannels_; ++ch)
            {
                std::copy(
                    &sample(ch, start),
                    &sample(ch, start + nSamples),
                    output[ch]
                );
            }
        }
    }
} // namespace artifact_remover::utils