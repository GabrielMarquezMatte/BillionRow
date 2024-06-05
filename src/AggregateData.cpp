#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <double-conversion/string-to-double.h>
#include <ankerl/unordered_dense.h>
#include <span>
#include <thread>
#include <spanstream>
#include <Windows.h>
#undef max
#undef min

class MemoryMappedFile final
{
public:
    MemoryMappedFile(const std::filesystem::path& filePath)
        : m_fileHandle(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
        , m_fileMappingHandle(CreateFileMappingW(m_fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr))
        , m_fileView(static_cast<const char*>(MapViewOfFile(m_fileMappingHandle, FILE_MAP_READ, 0, 0, 0)))
        , m_fileSize(static_cast<std::size_t>(GetFileSize(m_fileHandle, nullptr))),
        m_isOpen(m_fileHandle != INVALID_HANDLE_VALUE && m_fileMappingHandle != nullptr && m_fileView != nullptr)
    {
    }

    ~MemoryMappedFile()
    {
        UnmapViewOfFile(m_fileView);
        CloseHandle(m_fileMappingHandle);
        CloseHandle(m_fileHandle);
    }

    [[nodiscard]] inline const std::ispanstream GetSpanStream() const noexcept
    {
        return std::ispanstream(std::span(m_fileView, m_fileSize));
    }

    inline constexpr const std::size_t GetSize() const noexcept
    {
        return m_fileSize;
    }

    [[nodiscard]] inline constexpr bool IsOpen() const noexcept
    {
        return m_isOpen;
    }

    void Close() noexcept
    {
        UnmapViewOfFile(m_fileView);
        CloseHandle(m_fileMappingHandle);
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        m_fileMappingHandle = nullptr;
        m_fileView = nullptr;
        m_fileSize = 0;
        m_isOpen = false;
    }

    inline constexpr std::span<const char> GetSpan() const noexcept
    {
        return std::span(m_fileView, m_fileSize);
    }

private:
    HANDLE m_fileHandle;
    HANDLE m_fileMappingHandle;
    const char* m_fileView;
    std::size_t m_fileSize;
    bool m_isOpen;
};

struct Indicators final
{
    double MinValue;
    double MaxValue;
    double Sum;
    std::size_t Count;
};

void CalculateForLine(const std::string_view line, ankerl::unordered_dense::map<std::string, Indicators>& indicators)
{
    static const double_conversion::StringToDoubleConverter converter(
        double_conversion::StringToDoubleConverter::Flags::NO_FLAGS,
        0,
        NAN,
        "infinity",
        "nan"
    );
    size_t pos = line.find(';');
    std::string key = std::string(line.substr(0, pos));
    std::string_view value_string = line.substr(pos + 1);
    while(value_string.back() == '\r' || value_string.back() == '\n')
    {
        value_string.remove_suffix(1);
    }
    int processed = 0;
    double value = converter.StringToDouble(value_string.data(), static_cast<int>(value_string.size()), &processed);
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

constexpr void CalculateForSpan(const std::span<const char> span, ankerl::unordered_dense::map<std::string, Indicators>& indicators)
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

ankerl::unordered_dense::map<std::string, Indicators> MergeResults(const std::vector<ankerl::unordered_dense::map<std::string, Indicators>>& indicators)
{
    ankerl::unordered_dense::map<std::string, Indicators> result;
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

ankerl::unordered_dense::map<std::string, Indicators> CalculateForSpanThreaded(const std::span<const char> span, std::size_t threadCount)
{
    std::vector<std::thread> threads;
    std::vector<ankerl::unordered_dense::map<std::string, Indicators>> results(threadCount);
    std::size_t spanSize = span.size();
    std::size_t chunkSize = std::max(spanSize / threadCount, 1ULL);
    std::size_t start = 0;
    for(std::size_t i = 0; i < threadCount; i++)
    {
        std::size_t end = i == threadCount - 1 ? spanSize : start + chunkSize;
        while(end < spanSize && span[end] != '\n')
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
    MemoryMappedFile file(filePath);
    if(!file.IsOpen())
    {
        std::cout << "Could not open file\n";
        return 1;
    }
    auto start = std::chrono::high_resolution_clock::now();
    std::size_t count = 1;
    std::size_t threadCount = std::thread::hardware_concurrency() / 2;
    ankerl::unordered_dense::map<std::string, Indicators> indicators = CalculateForSpanThreaded(file.GetSpan(), threadCount);
    file.Close();
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