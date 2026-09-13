#pragma once

#include <filesystem>

class Repository {
public:
    void init();

private:
    std::filesystem::path findRepositoryRoot();
};