#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <format>
#include <print> 
#include <ranges>
#include <unordered_map>

std::string downloadFolder = "./downloads";

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out)
{
    size_t totalSize = size * nmemb;
    out->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string downloadFile(const std::string& url, const std::string& localPath) 
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Failed to initialize curl" << std::endl;
        return {};
    }
    std::string data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) 
    {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
    }
    else 
    {
        long res;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
        if (res != 200)
        {
            data.clear(); 
            std::print("downloading {} failed with status code {}\n", url, res);
        }
        else
        {
            // Save the content to the local file
            std::ofstream outfile(localPath);
            outfile << data;
            outfile.close();
        }
    }
    curl_easy_cleanup(curl); 
    return data;
}

std::string getFileContent(const std::string& year, const std::string& grade, int region, bool allowDownload)
{
    std::string url = std::format("http://www.mategye.hu/download/eredmenyek/{}/Z_{}_egyenieredmeny70_{}.txt", year, region, grade);
    std::string localFilePath = std::format("{}/{}_{}_{}.txt", downloadFolder, year, grade, region);

    if (std::filesystem::exists(localFilePath))
    {
        std::ifstream file(localFilePath);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(!content.empty())
        {
            return content;
        }
    }
    if (allowDownload)
    {
        return downloadFile(url, localFilePath);
    }
    else
    {
        return {};
    }
}

std::vector<std::string> splitLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream(content);

    while (std::getline(stream, line)) 
    {
        if (!line.empty() && line.back() == '\r') 
        {
            line.pop_back();
        }    
        if (!line.empty()) 
        {
            lines.push_back(line);
        }
    }
    return lines;
}

struct cResult
{
    std::string regionName;
    std::string name;
    std::string school;
    std::string city;
    int points;
    int prior;
};

struct cSchoolResult
{
    int points = 0;
    int prior = 0;
    int count = 0;
};


// Hely  Név                            Oszt.   Pont    Rossz   Prior   Iskola név                                    Helység              Tanarok    
// 0         1         2         3         4         5         6         7         8         9         10        11        12        13        14
// 012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890

int main(int argc, char* argv[]) 
{
    if (argc < 3)
    {
        std::cerr << "Hasznalat: zrinyi_osszesites <ev> <osztaly> [-no-download]" << std::endl;
        return 1;
    }

    std::string year = argv[1];
    std::string grade = argv[2];

    std::print("Ev: {}, Osztaly: {}\n", year, grade);

    bool allowDownload = true;
    if (argc == 4 && std::string(argv[3]) == "-no-download")
    {
        allowDownload = false;
    }

    std::filesystem::create_directory(downloadFolder);
    std::vector<cResult> results;

    for (int region = 10; region <= 35; ++region)
    {
        std::string content = getFileContent(year, grade, region, allowDownload);
        if (content.empty())
        {
            std::print("region #{} is missing\n", region);
            continue;
        }
        auto lines = splitLines(content);
        std::string regionName = lines[0];
        regionName = regionName.substr(regionName.find(':') + 2);
        std::print("region #{} is {}\n", region, regionName);
        for (auto& line : lines | std::views::drop(4))
        {
            if (line.find("Megye") == 0)
            {
                break;
            }
            auto& result = results.emplace_back();
            result.regionName = regionName;
            result.name = line.substr(6, 35-6);
            result.name = result.name.substr(0, result.name.find_last_not_of(' ') + 1);
            result.school = line.substr(69, 115 - 69);
            result.school = result.school.substr(0, result.school.find_last_not_of(' ') + 1);
            result.city = line.substr(115, 135 - 115);
            result.city = result.city.substr(0, result.city.find_last_not_of(' ') + 1);
            result.points = std::stoi(line.substr(45, 45 - 52));
            result.prior = std::stoi(line.substr(61, 68 - 61));
        }
    }
    std::ranges::sort(results, [](const cResult& a, const cResult& b) { return std::make_tuple(a.points, a.prior, b.name) > std::make_tuple(b.points, b.prior, a.name); });
    std::ofstream outfile(std::format("eredmenyek_{}_{}.txt", year, grade));
    std::unordered_map<std::string, cSchoolResult> schoolResults;
    for (auto&& [pos, result] : results | std::views::enumerate)
    {
        std::string resultText = 
            pos != 0 && results[pos].points == results[pos - 1].points && results[pos].prior == results[pos - 1].prior ? "   " : std::format("{:3}", pos + 1);

        auto& schoolResult = schoolResults[std::format("{} ({})", result.school, result.city)];
        if (schoolResult.count < 4)
        {
            schoolResult.points += result.points;
            schoolResult.prior += result.prior;
            ++schoolResult.count;
        }

        resultText += std::format(" {:35} {:3}  {:3}  {:50} {:20}\n", result.name, result.points, result.prior, result.school, result.city);

        std::print("{}", resultText);
        outfile << resultText;
    }
    std::string schoolResultHeader = "\n\n\nIskolak top 4 versenyzo alapjan : \n\n";
    outfile << schoolResultHeader;
    std::print("{}", schoolResultHeader);

    std::vector<std::pair<std::string, cSchoolResult>> sortedSchoolResults(schoolResults.begin(), schoolResults.end());
    std::ranges::sort(sortedSchoolResults, [](const auto& a, const auto& b) { return std::make_tuple(a.second.points, a.second.prior, b.first) > std::make_tuple(b.second.points, b.second.prior, a.first); });
    for (auto&& [pos, result] : sortedSchoolResults | std::views::enumerate)
    {
        std::string resultText = std::format("{:3} {:60} {:3}  {:3}\n", pos + 1, result.first, result.second.points, result.second.prior);
        std::print("{}", resultText);
        outfile << resultText;
    }

    return 0;
}
