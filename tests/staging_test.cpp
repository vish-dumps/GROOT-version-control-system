#include "catch.hpp"

#include "groot/index.hpp"
#include "groot/object_store.hpp"
#include "groot/repository.hpp"
#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

struct TemporaryDirectory {
    fs::path path;

    TemporaryDirectory() {
        path = fs::temp_directory_path() /
            ("groot-staging-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(path);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

bool has(const std::vector<std::string>& paths, const std::string& path) {
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}

} // namespace

TEST_CASE("index persists ordered paths and replaces staged hashes", "[index]") {
    TemporaryDirectory temporary;
    groot::Repository::init(temporary.path);
    groot::Index index(temporary.path);
    index.load();
    REQUIRE(index.entries().empty());

    const auto first = groot::SHA256::hash("first");
    const auto second = groot::SHA256::hash("second");
    index.set("z file.txt", first);
    index.set("src/a.cpp", second);
    index.save();

    groot::Index reloaded(temporary.path);
    reloaded.load();
    REQUIRE(reloaded.entries().at("z file.txt") == first);
    REQUIRE(reloaded.entries().at("src/a.cpp") == second);
    const auto serialized = groot::read_file(temporary.path / ".groot" / "index");
    REQUIRE(serialized.starts_with(second + " \"src/a.cpp\"\n"));

    reloaded.set("z file.txt", second);
    reloaded.save();
    index.load();
    REQUIRE(index.entries().size() == 2);
    REQUIRE(index.entries().at("z file.txt") == second);
    REQUIRE_THROWS(index.set("../outside", first));
    REQUIRE_THROWS(index.set(".groot/HEAD", first));
}

TEST_CASE("add stores bytes and status compares index with working files", "[staging]") {
    TemporaryDirectory temporary;
    groot::Repository::init(temporary.path);
    const auto file = temporary.path / "hello world.txt";
    groot::write_file(file, "hello");

    auto status = groot::Repository::status(temporary.path);
    REQUIRE(status.branch == "main");
    REQUIRE(has(status.untracked, "hello world.txt"));
    REQUIRE(status.staged.empty());

    REQUIRE(groot::Repository::add("hello world.txt", temporary.path) == 1);
    groot::Index index(temporary.path);
    index.load();
    REQUIRE(index.entries().at("hello world.txt") == groot::SHA256::hash("hello"));
    REQUIRE(groot::ObjectStore(temporary.path).load(index.entries().at("hello world.txt")) == "hello");

    status = groot::Repository::status(temporary.path);
    REQUIRE(has(status.staged, "hello world.txt"));
    REQUIRE(status.modified.empty());
    REQUIRE(status.untracked.empty());

    groot::write_file(file, "hello again");
    status = groot::Repository::status(temporary.path);
    REQUIRE(has(status.modified, "hello world.txt"));
    REQUIRE(groot::Repository::add("hello world.txt", temporary.path) == 1);
    index.load();
    REQUIRE(index.entries().at("hello world.txt") == groot::SHA256::hash("hello again"));
    REQUIRE(groot::Repository::status(temporary.path).modified.empty());
}

TEST_CASE("add dot recursively stages files and excludes metadata", "[staging]") {
    TemporaryDirectory temporary;
    groot::Repository::init(temporary.path);
    fs::create_directories(temporary.path / "src" / "nested");
    fs::create_directory(temporary.path / ".git");
    groot::write_file(temporary.path / "src" / "nested" / "a.txt", "same");
    groot::write_file(temporary.path / "b.txt", "same");
    groot::write_file(temporary.path / ".git" / "config", "ignored");

    REQUIRE(groot::Repository::add(".", temporary.path) == 2);
    groot::Index index(temporary.path);
    index.load();
    REQUIRE(index.entries().size() == 2);
    REQUIRE(index.entries().at("b.txt") == index.entries().at("src/nested/a.txt"));
    REQUIRE(fs::directory_iterator(temporary.path / ".groot" / "objects") != fs::directory_iterator{});
    REQUIRE(groot::Repository::status(temporary.path).untracked.empty());

    // Invoking add from a nested directory stores repository-relative paths.
    groot::write_file(temporary.path / "src" / "nested" / "new.txt", "new");
    REQUIRE(groot::Repository::add(".", temporary.path / "src" / "nested") == 2);
    index.load();
    REQUIRE(index.contains("src/nested/new.txt"));
    REQUIRE_FALSE(index.contains("new.txt"));
}

TEST_CASE("deleted indexed files are reported and can be staged for removal", "[staging]") {
    TemporaryDirectory temporary;
    groot::Repository::init(temporary.path);
    groot::write_file(temporary.path / "a.txt", "a");
    groot::write_file(temporary.path / "b.txt", "b");
    REQUIRE(groot::Repository::add(".", temporary.path) == 2);

    fs::remove(temporary.path / "a.txt");
    REQUIRE(has(groot::Repository::status(temporary.path).deleted, "a.txt"));
    REQUIRE(groot::Repository::add("a.txt", temporary.path) == 1);
    groot::Index index(temporary.path);
    index.load();
    REQUIRE_FALSE(index.contains("a.txt"));
    REQUIRE(index.contains("b.txt"));

    fs::remove(temporary.path / "b.txt");
    REQUIRE(groot::Repository::add(".", temporary.path) == 1);
    index.load();
    REQUIRE(index.entries().empty());
}

TEST_CASE("add rejects missing, outside, and metadata paths", "[staging]") {
    TemporaryDirectory temporary;
    const auto root = temporary.path / "repo";
    fs::create_directory(root);
    groot::Repository::init(root);
    groot::write_file(temporary.path / "outside.txt", "outside");

    REQUIRE_THROWS(groot::Repository::add("missing.txt", root));
    REQUIRE_THROWS(groot::Repository::add("../outside.txt", root));
    REQUIRE_THROWS(groot::Repository::add(".groot/HEAD", root));
    REQUIRE_THROWS(groot::Repository::status(temporary.path));
}
