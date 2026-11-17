#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#ifndef BENCH_OPT
#define BENCH_OPT 2
#endif

namespace {

constexpr std::size_t TOTAL_BITS = 4096;
constexpr std::size_t RUNS = 10;
constexpr std::size_t ITERATIONS = 200000;
const char *const OPT_LABEL = BENCH_OPT == 0 ? "-O0" : "-O2";

volatile std::uint64_t sink = 0;

template <typename Limb>
constexpr std::size_t limb_count = TOTAL_BITS / (sizeof(Limb) * 8);

template <typename Limb>
using Limbs = std::array<Limb, limb_count<Limb>>;

template <typename Limb>
Limbs<Limb> make_pattern(std::uint64_t seed)
{
    Limbs<Limb> value{};
    std::uint64_t state = seed;

    for (std::size_t i = 0; i < value.size(); ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        value[i] = static_cast<Limb>(
            state & static_cast<std::uint64_t>(
                        std::numeric_limits<Limb>::max()));
    }

    value[value.size() - 1] |= static_cast<Limb>(
        static_cast<Limb>(1) << (sizeof(Limb) * 8 - 1));

    return value;
}

template <typename Limb>
void add_limbs(const Limbs<Limb> &left, const Limbs<Limb> &right,
               Limbs<Limb> &result)
{
    Limb carry = 0;

    for (std::size_t i = 0; i < left.size(); ++i) {
        Limb first = static_cast<Limb>(left[i] + right[i]);
        Limb carry1 = static_cast<Limb>(first < left[i]);

        Limb second = static_cast<Limb>(first + carry);
        Limb carry2 = static_cast<Limb>(second < first);

        result[i] = second;
        carry = static_cast<Limb>(carry1 | carry2);
    }

    sink ^= static_cast<std::uint64_t>(carry);
}

template <typename Limb>
void double_limbs(const Limbs<Limb> &value, Limbs<Limb> &result)
{
    Limb carry = 0;

    for (std::size_t i = 0; i < value.size(); ++i) {
        Limb first = static_cast<Limb>(value[i] + value[i]);
        Limb carry1 = static_cast<Limb>(first < value[i]);

        Limb second = static_cast<Limb>(first + carry);
        Limb carry2 = static_cast<Limb>(second < first);

        result[i] = second;
        carry = static_cast<Limb>(carry1 | carry2);
    }

    sink ^= static_cast<std::uint64_t>(carry);
}

struct Stats {
    double minimum;
    double maximum;
    double mean;
    double median;
};

Stats calculate_stats(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());

    double sum = 0.0;
    for (double sample : samples)
        sum += sample;

    Stats stats{};
    stats.minimum = samples.front();
    stats.maximum = samples.back();
    stats.mean = sum / static_cast<double>(samples.size());

    const std::size_t middle = samples.size() / 2;
    if (samples.size() % 2 == 0)
        stats.median = (samples[middle - 1] + samples[middle]) / 2.0;
    else
        stats.median = samples[middle];

    return stats;
}

template <typename Limb>
std::vector<double> measure_addition(const Limbs<Limb> &left,
                                     const Limbs<Limb> &right)
{
    Limbs<Limb> result{};

    for (std::size_t i = 0; i < 1000; ++i)
        add_limbs(left, right, result);
    sink ^= static_cast<std::uint64_t>(result[0] ^ result[result.size() - 1]);

    std::vector<double> samples;
    samples.reserve(RUNS);

    for (std::size_t run = 0; run < RUNS; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < ITERATIONS; ++i) {
            add_limbs(left, right, result);
            sink ^= static_cast<std::uint64_t>(
                result[0] ^ result[result.size() - 1]);
        }
        const auto finish = std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start)
                .count();
        samples.push_back(static_cast<double>(elapsed) /
                          static_cast<double>(ITERATIONS));
    }

    return samples;
}

template <typename Limb>
std::vector<double> measure_doubling(const Limbs<Limb> &value)
{
    Limbs<Limb> result{};

    for (std::size_t i = 0; i < 1000; ++i)
        double_limbs(value, result);
    sink ^= static_cast<std::uint64_t>(result[0] ^ result[result.size() - 1]);

    std::vector<double> samples;
    samples.reserve(RUNS);

    for (std::size_t run = 0; run < RUNS; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < ITERATIONS; ++i) {
            double_limbs(value, result);
            sink ^= static_cast<std::uint64_t>(
                result[0] ^ result[result.size() - 1]);
        }
        const auto finish = std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start)
                .count();
        samples.push_back(static_cast<double>(elapsed) /
                          static_cast<double>(ITERATIONS));
    }

    return samples;
}

void print_stats(const char *type, const char *operation, const Stats &stats)
{
    std::printf("%-8s %-7s %-3s min=%9.3f max=%9.3f mean=%9.3f "
                "median=%9.3f ns/op\n",
                type, operation, OPT_LABEL,
                stats.minimum, stats.maximum, stats.mean, stats.median);
}

template <typename Limb>
void run_type(const char *type)
{
    const Limbs<Limb> left = make_pattern<Limb>(0x123456789abcdef0ULL);
    const Limbs<Limb> right = make_pattern<Limb>(0xfedcba9876543210ULL);

    const Stats addition = calculate_stats(measure_addition(left, right));
    const Stats doubling = calculate_stats(measure_doubling(left));

    print_stats(type, "add", addition);
    print_stats(type, "double", doubling);
}

} // namespace

int main()
{
    std::printf("=== BigUInt limb benchmark ===\n");
    std::printf("bits = %zu, iterations/run = %zu, runs = %zu, build = %s\n",
                TOTAL_BITS, ITERATIONS, RUNS, OPT_LABEL);
    std::printf("compiler = %s\n", __VERSION__);
    std::printf("\n");
    std::printf("%-8s %-7s %-3s %9s %9s %9s %9s\n",
                "type", "op", "opt", "min", "max", "mean", "median");
    std::printf("------------------------------------------------------------"
                "------------\n");

    run_type<std::uint16_t>("uint16");
    run_type<std::uint32_t>("uint32");
    run_type<std::uint64_t>("uint64");

    std::printf("\nsink = 0x%016llx\n",
                static_cast<unsigned long long>(sink));
    return 0;
}
