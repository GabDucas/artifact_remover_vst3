// RingBuffer.h

#include <vector>
#include <atomic>
#include <cstddef>
#include <algorithm>
#include <stdexcept>

namespace artifact_remover::streaming
{
    class RingBuffer
    {
    public:

        RingBuffer(size_t nChannels, size_t bufferSize);

        // Append block:
        // data shape = [nChannels][nSamples]
        void append(
            const float* const* data,
            size_t nSamples
        );

        // Read latest samples into preallocated output
        // output shape = [nChannels][nSamples]
        void getLatest(
            float* const* output,
            size_t nSamples
        ) const;

        // Read one channel sample
        inline float at(size_t channel, size_t idx) const
        {
            return buffer_[channel * size_ + idx];
        }

        inline size_t size() const noexcept
        {
            return size_;
        }

        inline size_t channels() const noexcept
        {
            return nChannels_;
        }

        inline size_t writePosition() const noexcept
        {
            return writePos_.load(std::memory_order_acquire);
        }

    private:

        inline float& sample(size_t ch, size_t idx)
        {
            return buffer_[ch * size_ + idx];
        }

        inline const float& sample(size_t ch, size_t idx) const
        {
            return buffer_[ch * size_ + idx];
        }

    private:

        size_t nChannels_;
        size_t size_;

        // contiguous memory
        std::vector<float> buffer_;

        // atomic for realtime-safe producer/consumer
        std::atomic<size_t> writePos_{0};

        bool full_{false};
    };
} // namespace artifact_remover::streaming