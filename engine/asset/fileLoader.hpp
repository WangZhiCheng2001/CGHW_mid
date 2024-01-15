#pragma once
/* simply copied from nvpro_cores/nvh */

#include <string>
#include <vector>
#include <fstream>

#include <spdlog/spdlog.h>

inline std::string findFile(const std::string &infilename, const std::vector<std::string> &directories, bool warn = false)
{
    std::ifstream stream;

    {
        stream.open(infilename.c_str());
        if (stream.is_open())
        {
            spdlog::info("Found file {}.", infilename);
            return infilename;
        }
    }

    for (const auto &directory : directories)
    {
        std::string filename = directory + "/" + infilename;
        stream.open(filename.c_str());
        if (stream.is_open())
        {
            spdlog::info("Found file {}.", filename);
            return filename;
        }
    }

    if (warn)
    {
        std::string all_directories{};
        for (const auto &directory : directories)
            all_directories += directory + " - ";
        spdlog::warn("File {} not found in directories {}.", infilename, all_directories);
    }

    return {};
}

inline std::string loadFile(const std::string &filename, bool binary)
{
    std::string result;
    std::ifstream stream(filename, std::ios::ate | (binary ? std::ios::binary : std::ios_base::openmode(0)));

    if (!stream.is_open())
    {
        return result;
    }

    result.reserve(stream.tellg());
    stream.seekg(0, std::ios::beg);

    result.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return result;
}

inline std::string loadFile(const char *filename, bool binary)
{
    std::string name(filename);
    return loadFile(name, binary);
}

inline std::string loadFile(const std::string &filename,
                            bool binary,
                            const std::vector<std::string> &directories,
                            std::string &filenameFound,
                            bool warn = false)
{
    filenameFound = findFile(filename, directories, warn);
    if (filenameFound.empty())
    {
        return {};
    }
    else
    {
        return loadFile(filenameFound, binary);
    }
}

inline std::string loadFile(const std::string filename, bool binary, const std::vector<std::string> &directories, bool warn = false)
{
    std::string filenameFound;
    return loadFile(filename, binary, directories, filenameFound, warn);
}