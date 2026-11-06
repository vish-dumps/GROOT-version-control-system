#include "catch.hpp"

#include "groot/repository.hpp"
#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
struct CommitTestDirectory {
    fs::path path = fs::temp_directory_path() /
        ("groot-commit-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CommitTestDirectory() { fs::create_directory(path); }
    ~CommitTestDirectory() { std::error_code ignored; fs::remove_all(path, ignored); }
};

bool contains(const std::vector<std::string>& paths, const std::string& path) {
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}
}

TEST_CASE("commits snapshot the index and log follows parent links", "[commit]") {
    CommitTestDirectory temporary;
    groot::Repository::init(temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).empty());
    REQUIRE_THROWS(groot::Repository::commit("empty", temporary.path));

    groot::write_file(temporary.path / "a.txt", "first");
    groot::Repository::add("a.txt", temporary.path);
    const auto first = groot::Repository::commit("first snapshot", temporary.path);
    REQUIRE(first.size() == 64);
    REQUIRE(groot::read_file(temporary.path / ".groot" / "refs" / "heads" / "main") == first + "\n");
    REQUIRE(groot::SHA256::hash(groot::read_file(temporary.path / ".groot" / "commits" / first)) == first);
    REQUIRE(groot::Repository::status(temporary.path).staged.empty());
    REQUIRE_THROWS(groot::Repository::commit("unchanged", temporary.path));

    groot::write_file(temporary.path / "a.txt", "second");
    REQUIRE(contains(groot::Repository::status(temporary.path).modified, "a.txt"));
    groot::Repository::add("a.txt", temporary.path);
    REQUIRE(contains(groot::Repository::status(temporary.path).staged_modified, "a.txt"));
    groot::write_file(temporary.path / "a.txt", "unstaged edit");
    const auto second = groot::Repository::commit("second snapshot", temporary.path);
    const auto history = groot::Repository::log(temporary.path);
    REQUIRE(history.size() == 2);
    REQUIRE(history[0].hash == second);
    REQUIRE(history[0].parent == first);
    REQUIRE(history[0].files.at("a.txt") == groot::SHA256::hash("second"));
    REQUIRE(contains(groot::Repository::status(temporary.path).modified, "a.txt"));
    REQUIRE(history[1].message == "first snapshot");
    REQUIRE(history[1].files.at("a.txt") == groot::SHA256::hash("first"));

    fs::remove(temporary.path / "a.txt");
    groot::Repository::add("a.txt", temporary.path);
    REQUIRE(contains(groot::Repository::status(temporary.path).staged_deleted, "a.txt"));
    const auto third = groot::Repository::commit("remove a", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().hash == third);
    REQUIRE(groot::Repository::log(temporary.path).front().files.empty());
    REQUIRE(groot::Repository::status(temporary.path).staged_deleted.empty());
}

TEST_CASE("corrupt commit records and invalid messages are rejected", "[commit]") {
    CommitTestDirectory temporary;
    groot::Repository::init(temporary.path);
    groot::write_file(temporary.path / "a.txt", "content");
    groot::Repository::add("a.txt", temporary.path);
    REQUIRE_THROWS(groot::Repository::commit("two\nlines", temporary.path));
    const auto hash = groot::Repository::commit("good", temporary.path);
    groot::write_file(temporary.path / ".groot" / "commits" / hash, "corrupt");
    REQUIRE_THROWS(groot::Repository::log(temporary.path));
    REQUIRE_THROWS(groot::Repository::status(temporary.path));
}
