#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "groot/object_store.hpp"
#include "groot/repository.hpp"
#include "groot/sha256.hpp"
#include "groot/utils.hpp"

#include <chrono>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

struct TemporaryDirectory {
    fs::path path;

    TemporaryDirectory() {
        path = fs::temp_directory_path() /
            ("groot-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(path);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

} // namespace

TEST_CASE("init creates the repository layout and discovery walks upward", "[repository]") {
    TemporaryDirectory temporary;
    REQUIRE_FALSE(groot::Repository::discover(temporary.path).has_value());

    const auto metadata = groot::Repository::init(temporary.path);
    REQUIRE(fs::is_directory(metadata / "objects"));
    REQUIRE(fs::is_directory(metadata / "commits"));
    REQUIRE(fs::is_directory(metadata / "refs" / "heads"));
    REQUIRE(groot::read_file(metadata / "HEAD") == "ref: refs/heads/main\n");
    REQUIRE(groot::read_file(metadata / "index").empty());
    REQUIRE(groot::read_file(metadata / "refs" / "heads" / "main").empty());

    const auto nested = temporary.path / "nested";
    fs::create_directory(nested);
    REQUIRE(groot::Repository::discover(nested) == fs::absolute(temporary.path));
    REQUIRE_THROWS(groot::Repository::init(temporary.path));
    REQUIRE_THROWS(groot::Repository::init(nested));
}

TEST_CASE("SHA-256 matches known vectors and is deterministic", "[sha256]") {
    REQUIRE(groot::SHA256::hash("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    REQUIRE(groot::SHA256::hash("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(groot::SHA256::hash("hello") ==
            "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    REQUIRE(groot::SHA256::hash("hello") == groot::SHA256::hash("hello"));
    REQUIRE(groot::SHA256::hash("hello") != groot::SHA256::hash("Hello"));
    REQUIRE(groot::SHA256::hash(std::string(1'000'000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("object storage round-trips bytes and deduplicates identical content", "[objects]") {
    TemporaryDirectory temporary;
    groot::Repository::init(temporary.path);
    groot::ObjectStore objects(temporary.path);

    const auto hello_hash = objects.store("hello");
    REQUIRE(hello_hash == groot::SHA256::hash("hello"));
    REQUIRE(objects.exists(hello_hash));
    REQUIRE(objects.load(hello_hash) == "hello");
    REQUIRE(objects.store("hello") == hello_hash);
    REQUIRE(std::distance(fs::directory_iterator(temporary.path / ".groot" / "objects"),
                          fs::directory_iterator{}) == 1);

    const std::string binary("a\0b\xff", 4);
    const auto binary_hash = objects.store(binary);
    REQUIRE(objects.load(binary_hash) == binary);
    REQUIRE(binary_hash != hello_hash);
    REQUIRE_THROWS(objects.load("../HEAD"));
    REQUIRE_THROWS(objects.load(std::string(64, '0')));
}

TEST_CASE("corrupted objects are rejected on read and reuse", "[objects]") {
    TemporaryDirectory temporary;
    groot::Repository::init(temporary.path);
    groot::ObjectStore objects(temporary.path);
    const auto hash = objects.store("hello");

    groot::write_file(temporary.path / ".groot" / "objects" / hash, "corrupted");
    REQUIRE_THROWS(objects.load(hash));
    REQUIRE_THROWS(objects.store("hello"));
}
