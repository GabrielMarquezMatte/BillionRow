#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <span>
#include <thread>
#include <mio/mmap.hpp>

struct Indicators final
{
    double MinValue;
    double MaxValue;
    double Sum;
    std::size_t Count;
};

static const std::size_t powers[] = {1, 31, 961, 29791, 923521, 28629151, 887503681, 27512614111};

struct CustomHash final
{
    constexpr inline std::uint64_t operator()(const std::string_view key) const noexcept
    {
        std::uint64_t hash = 0;
        const std::size_t keySize = key.size();
        const std::uint64_t size = keySize > 8 ? 8 : keySize;
        for(std::uint64_t i = 0; i < size; i++)
        {
            hash += key[i] * powers[i];
        }
        return hash;
    }
};

constexpr inline static double ParseDouble(const std::string_view value)
{
    std::size_t result = 0;
    int fractionalLength = 0;
    bool negative = value[0] == '-';
    bool decimalPoint = false;
    for(std::size_t i = negative; i < value.size(); i++)
    {
        char c = value[i];
        if(c == '.')
        {
            decimalPoint = true;
            continue;
        }
        result = result * 10 + (c - '0');
        if(decimalPoint)
        {
            fractionalLength++;
        }
    }
    if(fractionalLength > 2)
    {
        std::unreachable();
    }
    double finalResult = static_cast<double>(result);
    while(fractionalLength--)
    {
        finalResult *= 0.1;
    }
    return negative ? -finalResult : finalResult;
}

static void CalculateForLine(const std::string_view line, ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash>& indicators)
{
    std::size_t pos = line.find(';');
    std::string_view key = line.substr(0, pos);
    std::string_view value_string = line.substr(pos + 1);
    double value = ParseDouble(value_string);
    auto [it, inserted] = indicators.try_emplace(std::move(key), Indicators{value, value, value, 1});
    if(inserted)
    {
        return;
    }
    Indicators& indicator = it->second;
    indicator.MinValue = std::min(indicator.MinValue, value);
    indicator.MaxValue = std::max(indicator.MaxValue, value);
    indicator.Sum += value;
    indicator.Count++;
}

constexpr inline static std::size_t FindNewLineOffset(const std::span<const char> span, std::size_t offset)
{
    for(std::size_t i = offset; i < span.size(); i++)
    {
        if(span[i] == '\n')
        {
            return i;
        }
    }
    return span.size();
}

static void CalculateForSpan(const std::span<const char> span, ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash>& indicators)
{
    indicators.reserve(10'000);
    std::size_t offset = 0;
    while(offset < span.size())
    {
        std::size_t newLineOffset = FindNewLineOffset(span, offset);
        if(newLineOffset == std::string::npos)
        {
            break;
        }
        std::string_view line = std::string_view(span.data() + offset, newLineOffset - offset);
        CalculateForLine(line, indicators);
        offset = newLineOffset + 1;
    }
}

static ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash> MergeResults(const std::vector<ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash>>& indicators)
{
    ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash> result;
    result.reserve(10'000);
    for(const auto& db : indicators)
    {
        for(const auto& [key, indicator] : db)
        {
            auto [it, inserted] = result.try_emplace(std::move(key), indicator);
            if(inserted)
            {
                continue;
            }
            Indicators& resultIndicator = it->second;
            resultIndicator.MinValue = std::min(resultIndicator.MinValue, indicator.MinValue);
            resultIndicator.MaxValue = std::max(resultIndicator.MaxValue, indicator.MaxValue);
            resultIndicator.Sum += indicator.Sum;
            resultIndicator.Count += indicator.Count;
        }
    }
    return result;
}

static ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash> CalculateForSpanThreaded(const mio::mmap_source& file, std::size_t threadCount)
{
    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    std::vector<ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash>> results(threadCount);
    std::span<const char> span(file.data(), file.size());
    std::size_t spanSize = span.size();
    std::size_t maxChunkSize = spanSize / threadCount;
    std::size_t chunkSize = maxChunkSize < 1ULL ? 1ULL : maxChunkSize;
    std::size_t start = 0;
    for(std::size_t i = 0; i < threadCount; i++)
    {
        std::size_t end = i == threadCount - 1 ? spanSize : start + chunkSize;
        while (end < spanSize && span[end] != '\n')
        {
            end++;
        }
        if(start >= end || end > spanSize)
        {
            break;
        }
        threads.emplace_back([start, end, &span, &results, i]()
        {
            CalculateForSpan(span.subspan(start, end - start), results[i]);
        });
        start = end + 1;
    }
    for(auto& thread : threads)
    {
        thread.join();
    }
    return MergeResults(results);
}

int main()
{
    std::filesystem::path filePath = std::filesystem::current_path() / "data" / "data.csv";
    if(!std::filesystem::exists(filePath))
    {
        std::cout << "File does not exist\n";
        return 1;
    }
    auto start = std::chrono::high_resolution_clock::now();
    std::error_code ec;
    mio::mmap_source file = mio::make_mmap_source(filePath.string(), 0, mio::map_entire_file, ec);
    if(ec)
    {
        std::cout << "Could not map file: " << ec.message() << '\n';
        return 1;
    }
    if(!file.is_open())
    {
        std::cout << "Could not open file\n";
        return 1;
    }
    std::cout << "File size: " << file.size() << '\n';
    std::size_t count = 0;
    std::size_t threadCount = std::thread::hardware_concurrency();
    ankerl::unordered_dense::map<std::string_view, Indicators, CustomHash> indicators = CalculateForSpanThreaded(file, threadCount);
    std::filesystem::path outputPath = std::filesystem::current_path() / "data" / "output.csv";
    std::ofstream output(outputPath, std::ios::trunc | std::ios::out);
    if(!output.is_open())
    {
        std::cout << "Could not open output file\n";
        return 1;
    }
    for(const auto& [key, indicator] : indicators)
    {
        output << key << ';' << indicator.MinValue << ';' << indicator.MaxValue << ';' << indicator.Sum / indicator.Count << '\n';
        count += indicator.Count;
    }
    output.close();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Number of lines: " << count << '\n';
#if defined(_MSC_VER)
    std::cout << "Elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start) << '\n';
#else
    std::cout << "Elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms\n";
#endif
    return 0;
}