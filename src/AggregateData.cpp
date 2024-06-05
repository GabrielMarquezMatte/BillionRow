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
    constexpr inline std::size_t operator()(const std::string& key) const noexcept
    {
        std::size_t hash = 0;
        const std::size_t size = std::min(key.size(), 8ULL);
        for(std::size_t i = 0; i < size; i++)
        {
            hash += key[i] * powers[i];
        }
        return hash;
    }
};

constexpr inline double ParseDouble(const std::string_view value)
{
    double result = 0.0;
    double decimal = 0.1;
    bool negative = value[0] == '-';
    bool decimalPoint = false;
    for(std::size_t i = negative; i < value.size(); i++)
    {
        if(value[i] == '.')
        {
            decimalPoint = true;
            continue;
        }
        if(decimalPoint)
        {
            result += (value[i] - '0') * decimal;
            decimal *= 0.1;
        }
        else
        {
            result = result * 10 + (value[i] - '0');
        }
    }
    return negative ? -result : result;
}

void CalculateForLine(const std::string_view line, ankerl::unordered_dense::map<std::string, Indicators, CustomHash>& indicators)
{
    std::size_t pos = line.find(';');
    std::string key = std::string(line.substr(0, pos));
    std::string_view value_string = line.substr(pos + 1);
    while(value_string.back() == '\r' || value_string.back() == '\n')
    {
        value_string.remove_suffix(1);
    }
    double value = ParseDouble(value_string);
    auto [it, inserted] = indicators.try_emplace(key, Indicators{value, value, value, 1});
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

inline constexpr std::size_t FindNewLineOffset(const std::span<const char> span, std::size_t offset)
{
    const char *data = span.data();
    const char* start = data + offset;
    const char* end = data + span.size();
    const char* newLine = std::find(start, end, '\n');
    return newLine == end ? span.size() : newLine - data;
}

constexpr void CalculateForSpan(const std::span<const char> span, ankerl::unordered_dense::map<std::string, Indicators, CustomHash>& indicators)
{
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

ankerl::unordered_dense::map<std::string, Indicators, CustomHash> MergeResults(const std::vector<ankerl::unordered_dense::map<std::string, Indicators, CustomHash>>& indicators)
{
    ankerl::unordered_dense::map<std::string, Indicators, CustomHash> result;
    for(const auto& db : indicators)
    {
        for(const auto& [key, indicator] : db)
        {
            auto [it, inserted] = result.try_emplace(key, indicator);
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

ankerl::unordered_dense::map<std::string, Indicators, CustomHash> CalculateForSpanThreaded(const mio::mmap_source& file, std::size_t threadCount)
{
    std::vector<std::thread> threads;
    std::vector<ankerl::unordered_dense::map<std::string, Indicators, CustomHash>> results(threadCount);
    std::span<const char> span(file.data(), file.size());
    std::size_t spanSize = span.size();
    std::size_t chunkSize = std::max(spanSize / threadCount, 1ULL);
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
    mio::mmap_source file(filePath.string());
    if(!file.is_open())
    {
        std::cout << "Could not open file\n";
        return 1;
    }
    std::cout << "File size: " << file.size() << '\n';
    std::size_t count = 0;
    std::size_t threadCount = std::thread::hardware_concurrency();
    ankerl::unordered_dense::map<std::string, Indicators, CustomHash> indicators = CalculateForSpanThreaded(file, threadCount);
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
    std::cout << "Elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start) << '\n';
    return 0;
}