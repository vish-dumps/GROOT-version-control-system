#include "Repository.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;


//finding the nearest .groot directory

fs::path Repository::findRepositoryRoot() {

    fs::path current = fs::current_path();

    while (true) {

        if (fs::exists(current / ".groot") &&
            fs::is_directory(current / ".groot")) {

            return current;
            }

        if (current == current.root_path()) {
            break;
        }

        current = current.parent_path();
    }

    return {};
}

std::string simpleHash(const std::string& content) {

    std::hash<std::string> hasher;

    std::size_t hashValue = hasher(content);

    std::stringstream stream;

    stream << std::hex
           << std::setw(16)
           << std::setfill('0')
           << hashValue;

    return stream.str();
}

//groot log
void Repository::log() {

    fs::path root = findRepositoryRoot();

    if (root.empty()) {
        std::cout << "fatal: not a Groot repository\n";
        return;
    }

    fs::path headPath = root / ".groot" / "HEAD";

    std::ifstream headFile(headPath);

    if (!headFile) {
        std::cout << "Error: Could not read HEAD.\n";
        return;
    }

    std::string headContent;
    std::getline(headFile, headContent);

    const std::string prefix = "ref: refs/heads/";

    if (headContent.rfind(prefix, 0) != 0) {
        std::cout << "Error: Invalid HEAD format.\n";
        return;
    }

    std::string branchName =
        headContent.substr(prefix.size());

    fs::path branchPath =
        root / ".groot" / "refs" / "heads" / branchName;

    std::ifstream branchFile(branchPath);

    if (!branchFile) {
        std::cout << "No commits yet.\n";
        return;
    }

    std::string commitHash;
    std::getline(branchFile, commitHash);

    if (commitHash.empty()) {
        std::cout << "No commits yet.\n";
        return;
    }

    fs::path objectPath =
        root / ".groot" / "objects" / commitHash;

    std::ifstream objectFile(objectPath);

    if (!objectFile) {
        std::cout << "Error: Commit object not found.\n";
        return;
    }

    std::cout << "commit " << commitHash << '\n';

    std::string line;

    while (std::getline(objectFile, line)) {

        if (line.rfind("message: ", 0) == 0) {

            std::cout << "    "
                      << line.substr(9)
                      << '\n';

            break;
        }
    }

    std::cout << '\n';
}

//groot status
void Repository::status() {

    fs::path root = findRepositoryRoot();

    if (root.empty()) {
        std::cout << "fatal: not a Groot repository\n";
        return;
    }

    fs::path headPath = root / ".groot" / "HEAD";

    std::ifstream headFile(headPath);

    if (!headFile) {
        std::cout << "Error: Could not read HEAD.\n";
        return;
    }

    std::string headContent;
    std::getline(headFile, headContent);

    const std::string prefix = "ref: refs/heads/";

    if (headContent.rfind(prefix, 0) == 0) {

        std::string branchName =
            headContent.substr(prefix.size());

        std::cout << "On Groot branch: "
                  << branchName << '\n';

    } else {

        std::cout << "Error: Invalid HEAD format.\n";
        return;
    }

    fs::path indexPath = root / ".groot" / "index";

    std::ifstream indexFile(indexPath);

    if (!indexFile) {
        std::cout << "Error: Could not read index.\n";
        return;
    }

    std::string stagedFile;
    bool hasStagedFiles = false;

    while (std::getline(indexFile, stagedFile)) {

        if (stagedFile.empty()) {
            continue;
        }

        if (!hasStagedFiles) {
            std::cout << "\nChanges to be committed:\n";
            hasStagedFiles = true;
        }

        std::cout << "    " << stagedFile << '\n';
    }

    if (!hasStagedFiles) {
        std::cout << "Nothing to commit, working tree clean.\n";
    }
}

// groot commit
void Repository::commit(const std::string& message) {

    fs::path root = findRepositoryRoot();

    if (root.empty()) {
        std::cout << "fatal: not a Groot repository\n";
        return;
    }

    fs::path indexPath = root / ".groot" / "index";

    std::ifstream indexFile(indexPath);

    if (!indexFile) {
        std::cout << "Error: Could not read index.\n";
        return;
    }

    std::string stagedFile;
    std::stringstream commitContent;

    bool hasStagedFiles = false;

    while (std::getline(indexFile, stagedFile)) {

        if (stagedFile.empty()) {
            continue;
        }

        hasStagedFiles = true;

        commitContent << stagedFile << '\n';
    }

    indexFile.close();

    if (!hasStagedFiles) {
        std::cout << "Nothing to commit.\n";
        return;
    }

    commitContent << "message: " << message << '\n';

    std::string content = commitContent.str();

    std::string commitHash = simpleHash(content);

    fs::path objectPath =
        root / ".groot" / "objects" / commitHash;

    std::ofstream objectFile(objectPath);

    if (!objectFile) {
        std::cout << "Error: Could not create commit object.\n";
        return;
    }

    objectFile << content;
    objectFile.close();

    fs::path headPath = root / ".groot" / "HEAD";

    std::ifstream headFile(headPath);

    if (!headFile) {
        std::cout << "Error: Could not read HEAD.\n";
        return;
    }

    std::string headContent;
    std::getline(headFile, headContent);

    const std::string prefix = "ref: refs/heads/";

    if (headContent.rfind(prefix, 0) != 0) {
        std::cout << "Error: Invalid HEAD format.\n";
        return;
    }

    std::string branchName =
        headContent.substr(prefix.size());

    fs::path branchPath =
        root / ".groot" / "refs" / "heads" / branchName;

    std::ofstream branchFile(branchPath);

    if (!branchFile) {
        std::cout << "Error: Could not update branch.\n";
        return;
    }

    branchFile << commitHash << '\n';

    std::cout << "["
              << branchName
              << " "
              << commitHash
              << "] "
              << message
              << '\n';
}

//groot add
void Repository::add(const std::string& fileName) {

    fs::path root = findRepositoryRoot();

    if (root.empty()) {
        std::cout << "fatal: not a Groot repository\n";
        return;
    }

    fs::path filePath = fs::current_path() / fileName;

    if (!fs::exists(filePath)) {
        std::cout << "fatal: pathspec '"
                  << fileName
                  << "' did not match any files\n";
        return;
    }

    if (!fs::is_regular_file(filePath)) {
        std::cout << "fatal: '"
                  << fileName
                  << "' is not a regular file\n";
        return;
    }

    fs::path indexPath = root / ".groot" / "index";

    std::ifstream indexRead(indexPath);

    if (!indexRead) {
        std::cout << "Error: Could not read index.\n";
        return;
    }

    std::string stagedFile;
    bool alreadyStaged = false;

    while (std::getline(indexRead, stagedFile)) {

        if (stagedFile == fileName) {
            alreadyStaged = true;
            break;
        }
    }

    indexRead.close();

    if (alreadyStaged) {
        std::cout << "File already staged: "
                  << fileName << '\n';
        return;
    }

    std::ofstream index(indexPath, std::ios::app);

    if (!index) {
        std::cout << "Error: Could not open index.\n";
        return;
    }

    index << fileName << '\n';

    std::cout << "Added file: "
              << fileName << '\n';
}

//groot init
void Repository::init() {

    fs::path root = findRepositoryRoot();

    if (!root.empty()) {
        std::cout << "Repository found at: "
                  << root << '\n';
        return;
    }

    std::cout << "No Groot repository found.\n";

    fs::path repo = ".groot";

    if (fs::exists(repo)) {

        if (fs::is_directory(repo)) {
            std::cout << "Groot repository already exists.\n";
        } else {
            std::cout << "Error: .groot exists but is not a directory.\n";
        }

        return;
    }

    try {

        fs::create_directories(repo / "objects");
        fs::create_directories(repo / "refs" / "heads");

        std::ofstream head(repo / "HEAD");
        std::ofstream index(repo / "index");
        std::ofstream mainBranch(repo / "refs" / "heads" / "main");

        if (!head || !index || !mainBranch) {
            std::cout << "Error: Could not create repository files.\n";
            return;
        }
        head<<"ref: refs/heads/main\n";

        std::cout << "Initialized empty Groot repository.\n";

    } catch (const fs::filesystem_error& e) {

        std::cout << "Filesystem error: "
                  << e.what() << '\n';
    }
}

