#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <double-conversion/double-to-string.h>

struct Indicators
{
    double MinValue;
    double MaxValue;
    double Sum;
    std::size_t Count;
};

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
    std::unordered_map<std::string, Indicators> indicators;
    std::string buffer;
    while(std::getline(file, buffer))
    {
        std::string_view line(buffer);
        size_t pos = line.find(';');
        std::string key = buffer.substr(0, pos);
        double value = std::stod(line.substr(pos + 1).data());
        auto [it, inserted] = indicators.try_emplace(key, Indicators{value, value, value, 1});
        if(inserted)
        {
            continue;
        }
        Indicators& indicator = it->second;
        if(value < indicator.MinValue)
        {
            indicator.MinValue = value;
        }
        if(value > indicator.MaxValue)
        {
            indicator.MaxValue = value;
        }
        indicator.Sum += value;
        indicator.Count++;
    }
    file.close();
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
    }
    output.close();
    return 0;
}