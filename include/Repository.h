#pragma once

#include <filesystem>

class Repository {
public:
    void init();
    void status();
    void add(const std::string& fileName);
    void commit(const std::string& message);
    void log();

private:
    std::filesystem::path findRepositoryRoot();
};