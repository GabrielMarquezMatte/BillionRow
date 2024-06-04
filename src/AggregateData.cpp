#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <double-conversion/string-to-double.h>
#include <ankerl/unordered_dense.h>

struct Indicators
{
    double MinValue;
    double MaxValue;
    double Sum;
    std::size_t Count;
};

void CalculateForLine(const std::string_view line, ankerl::unordered_dense::map<std::size_t, Indicators>& indicators)
{
    static const double_conversion::StringToDoubleConverter converter(
        double_conversion::StringToDoubleConverter::Flags::NO_FLAGS,
        0,
        NAN,
        "infinity",
        "nan"
    );
    static const std::hash<std::string_view> hash;
    size_t pos = line.find(';');
    std::size_t key = hash(line.substr(0, pos));
    std::string_view value_string = line.substr(pos + 1);
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

int main()
{
    std::filesystem::path filePath = std::filesystem::current_path() / "data" / "data.csv";
    if(!std::filesystem::exists(filePath))
    {
        std::cout << "File does not exist\n";
        return 1;
    }
    std::ifstream file(filePath);
    if(!file.is_open())
    {
        std::cout << "Could not open file\n";
        return 1;
    }
    ankerl::unordered_dense::map<std::size_t, Indicators> indicators;
    std::stringstream buffer;
    buffer << file.rdbuf();
    char line[1024] = {0};
    auto start = std::chrono::high_resolution_clock::now();
    while(buffer.getline(line, sizeof(line) - 1, '\n'))
    {
        CalculateForLine(line, indicators);
    }
    file.close();
    std::filesystem::path outputPath = std::filesystem::current_path() / "data" / "output.csv";
    std::ofstream output(outputPath, std::ios::trunc | std::ios::out);
    if(!output.is_open())
    {
        std::cout << "Could not open output file\n";
        return 1;
    }
    std::size_t count = 1;
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