#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <format>
#include <print> 
#include <ranges>

// Callback function to store the data in a string
size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) 
{
    size_t totalSize = size * nmemb;
    out->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Function to check if the file exists locally
bool fileExists(const std::string& filepath) 
{
    return std::filesystem::exists(filepath);
}

// Function to load the content from a local file
std::string loadFromFile(const std::string& filepath) 
{
    std::ifstream file(filepath);
    std::string content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    return content;
}

// Function to download the file from the URL and save it locally
std::string downloadFile(const std::string& url, const std::string& localPath) 
{
    CURL* curl = curl_easy_init();
    std::string data;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

        // Perform the synchronous request
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
                data.clear(); // Clear the data if the download failed
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

        curl_easy_cleanup(curl); // Clean up after the operation
    }
    return data;
}

std::string getFileContent(const std::string& url, const std::string& downloadFolder, bool allowDownload) 
{
    // Extract the file name from the URL (you could modify this as needed)
    std::string filename = url.substr(url.find_last_of("/") + 1);

    // Combine the folder and file name to get the full path
    std::string localFilePath = downloadFolder + "/" + filename;

    if (fileExists(localFilePath)) 
    {
        return loadFromFile(localFilePath);
    }
    else 
    {
        if (allowDownload)
        {
            return downloadFile(url, localFilePath);
        }
        else
        {
            return {};
        }
    }
}

std::vector<std::string> splitLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream(content);

    // Read each line from the stringstream
    while (std::getline(stream, line)) {
        // If the line is not empty, add it to the vector
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }    
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

struct cResult
{
    std::string zoneName;
    std::string name;
    std::string school;
    std::string city;
    int points;
    int prior;
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

    // Define the download folder
    std::string downloadFolder = "./downloads";
    // Ensure the download folder exists
    std::filesystem::create_directory(downloadFolder);
    std::vector<cResult> results;

    for (int z = 10; z < 40; ++z)
    {

        // Create the URL using C++23 format
        std::string url = std::format("http://www.mategye.hu/download/eredmenyek/{}/Z_{}_egyenieredmeny70_{}.txt", year, z, grade);

        // Get the content (either from the local file or by downloading)
        std::string content = getFileContent(url, downloadFolder, allowDownload);
        if (content.empty())
        {
            std::print("zone #{} is missing\n", z);
            continue;
        }

        auto lines = splitLines(content);
        std::string zoneName = lines[0];
        zoneName = zoneName.substr(zoneName.find(':') + 2);

        std::print("Zone #{} is {}\n", z, zoneName);
        for (auto& line : lines | std::views::drop(4))
        {
            auto& result = results.emplace_back();
            result.zoneName = zoneName;
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
    std::ranges::sort(results, [](const cResult& a, const cResult& b) { return std::make_pair(a.points, a.prior) > std::make_pair(b.points, b.prior); });
    // write out to file
    std::ofstream outfile("eredmenyek.txt");
    for (auto&& [pos, result] : results | std::views::enumerate)
    {
        // pos, name, points, prior, schoold, city
        std::print("{:3} {:35} {:3}  {:3}  {:45} {:20}\n", pos + 1, result.name, result.points, result.prior, result.school, result.city);
        outfile << std::format("{:3} {:35} {:3}  {:3}  {:45} {:20}\n", pos + 1, result.name, result.points, result.prior, result.school, result.city);
    }

    return 0;
}
