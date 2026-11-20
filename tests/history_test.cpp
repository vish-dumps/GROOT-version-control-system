#include "catch.hpp"

#include "groot/diff.hpp"
#include "groot/repository.hpp"
#include "groot/utils.hpp"

#include <chrono>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {
struct HistoryDirectory {
    fs::path path = fs::temp_directory_path() /
        ("groot-history-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    HistoryDirectory() { fs::create_directory(path); groot::Repository::init(path); }
    ~HistoryDirectory() { std::error_code ignored; fs::remove_all(path, ignored); }
    void put(const std::string& path_name, const std::string& bytes) {
        const auto file = path / path_name;
        fs::create_directories(file.parent_path());
        groot::write_file(file, bytes);
        groot::Repository::add(path_name, path);
    }
};
}

TEST_CASE("LCS diff shows staged and working changes", "[diff]") {
    HistoryDirectory temporary;
    temporary.put("a.txt", "one\ntwo\nthree\n");
    REQUIRE(groot::Repository::diff(true, temporary.path).find("+one") != std::string::npos);
    groot::Repository::commit("base", temporary.path);
    REQUIRE(groot::Repository::diff(true, temporary.path).empty());
    groot::write_file(temporary.path / "a.txt", "one\nTWO\nthree\n");
    const auto working = groot::Repository::diff(false, temporary.path);
    REQUIRE(working.find("-two\n") != std::string::npos);
    REQUIRE(working.find("+TWO\n") != std::string::npos);
    groot::Repository::add("a.txt", temporary.path);
    REQUIRE(groot::Repository::diff(false, temporary.path).empty());
    REQUIRE(groot::Repository::diff(true, temporary.path).find("+TWO\n") != std::string::npos);
    REQUIRE(groot::line_diff(std::string("a\0b", 3), "text", "binary").find("Binary files differ") != std::string::npos);
}

TEST_CASE("checkout restores snapshots and protects tracked and untracked changes", "[branch]") {
    HistoryDirectory temporary;
    temporary.put("a.txt", "base");
    const auto base = groot::Repository::commit("base", temporary.path);
    groot::Repository::create_branch("feature", temporary.path);
    REQUIRE_THROWS(groot::Repository::create_branch("../bad", temporary.path));
    REQUIRE_THROWS(groot::Repository::create_branch("CON", temporary.path));
    REQUIRE_THROWS(groot::Repository::create_branch("trailing.", temporary.path));
    REQUIRE_THROWS(groot::Repository::create_branch("feature", temporary.path));
    groot::Repository::checkout("feature", temporary.path);
    temporary.put("a.txt", "feature");
    temporary.put("new.txt", "new");
    groot::Repository::commit("feature work", temporary.path);
    groot::write_file(temporary.path / "a.txt", "local edit");
    REQUIRE_THROWS(groot::Repository::checkout("main", temporary.path));
    groot::write_file(temporary.path / "a.txt", "feature");
    groot::Repository::checkout("main", temporary.path);
    REQUIRE(groot::Repository::status(temporary.path).branch == "main");
    REQUIRE(groot::read_file(temporary.path / "a.txt") == "base");
    REQUIRE_FALSE(fs::exists(temporary.path / "new.txt"));
    REQUIRE(groot::Repository::log(temporary.path).front().hash == base);
    groot::write_file(temporary.path / "new.txt", "untracked");
    REQUIRE_THROWS(groot::Repository::checkout("feature", temporary.path));
    REQUIRE(groot::read_file(temporary.path / "new.txt") == "untracked");
}

TEST_CASE("historical checkout returns to branch tip and supports branching", "[branch]") {
    HistoryDirectory temporary;
    temporary.put("a.txt", "first");
    const auto first = groot::Repository::commit("first", temporary.path);
    temporary.put("a.txt", "latest");
    const auto latest = groot::Repository::commit("latest", temporary.path);

    groot::Repository::checkout(first, temporary.path);
    REQUIRE(groot::Repository::status(temporary.path).detached);
    REQUIRE(groot::Repository::log(temporary.path).front().hash == first);
    REQUIRE(groot::read_file(temporary.path / "a.txt") == "first");
    REQUIRE_THROWS(groot::Repository::commit("cannot commit detached", temporary.path));

    groot::write_file(temporary.path / "a.txt", "local edit");
    REQUIRE_THROWS(groot::Repository::checkout("--latest", temporary.path));
    groot::write_file(temporary.path / "a.txt", "first");
    groot::Repository::checkout("--latest", temporary.path);
    REQUIRE_FALSE(groot::Repository::status(temporary.path).detached);
    REQUIRE(groot::Repository::log(temporary.path).front().hash == latest);
    REQUIRE(groot::read_file(temporary.path / "a.txt") == "latest");

    groot::Repository::checkout(first, temporary.path);
    groot::Repository::create_branch("from-old", temporary.path);
    groot::Repository::checkout("from-old", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().hash == first);
    temporary.put("a.txt", "branch work");
    groot::Repository::commit("branch work", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().hash == latest);
    REQUIRE_THROWS(groot::Repository::checkout("--latest", temporary.path));
}

TEST_CASE("checkout handles file and directory path transitions", "[branch]") {
    HistoryDirectory temporary;
    temporary.put("seed.txt", "seed");
    groot::Repository::commit("base", temporary.path);
    groot::Repository::create_branch("nested", temporary.path);
    groot::Repository::checkout("nested", temporary.path);
    temporary.put("folder/sub/a.txt", "nested");
    groot::Repository::commit("nested file", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    temporary.put("folder", "flat");
    groot::Repository::commit("flat file", temporary.path);
    groot::Repository::checkout("nested", temporary.path);
    REQUIRE(groot::read_file(temporary.path / "folder" / "sub" / "a.txt") == "nested");
    groot::Repository::checkout("main", temporary.path);
    REQUIRE(groot::read_file(temporary.path / "folder") == "flat");
}

TEST_CASE("merge fast-forwards and records two parents after divergent edits", "[merge]") {
    HistoryDirectory temporary;
    temporary.put("shared.txt", "a\nb\nc\n");
    groot::Repository::commit("base", temporary.path);
    groot::Repository::create_branch("feature", temporary.path);
    groot::Repository::checkout("feature", temporary.path);
    temporary.put("shared.txt", "A\nb\nc\n");
    const auto feature_tip = groot::Repository::commit("feature edit", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    const auto ff = groot::Repository::merge("feature", temporary.path);
    REQUIRE(ff.kind == groot::MergeResult::Kind::FastForward);
    REQUIRE(ff.hash == feature_tip);
    REQUIRE(groot::read_file(temporary.path / "shared.txt") == "A\nb\nc\n");
    REQUIRE(groot::Repository::merge("feature", temporary.path).kind == groot::MergeResult::Kind::AlreadyUpToDate);

    groot::Repository::checkout("feature", temporary.path);
    temporary.put("new.txt", "feature-only");
    const auto next_feature = groot::Repository::commit("feature add", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    temporary.put("shared.txt", "A\nb\nC\n");
    const auto main_tip = groot::Repository::commit("main edit", temporary.path);
    const auto merged = groot::Repository::merge("feature", temporary.path);
    REQUIRE(merged.kind == groot::MergeResult::Kind::Merged);
    REQUIRE(groot::Repository::log(temporary.path).front().parent == main_tip);
    REQUIRE(groot::Repository::log(temporary.path).front().merge_parent == next_feature);
    REQUIRE(groot::read_file(temporary.path / "new.txt") == "feature-only");
    REQUIRE(groot::Repository::status(temporary.path).staged.empty());
}

TEST_CASE("same-file disjoint edits combine and conflicts require staged resolution", "[merge]") {
    HistoryDirectory temporary;
    temporary.put("a.txt", "one\ntwo\nthree\n");
    groot::Repository::commit("base", temporary.path);
    groot::Repository::create_branch("feature", temporary.path);
    groot::Repository::checkout("feature", temporary.path);
    temporary.put("a.txt", "ONE\ntwo\nthree\n");
    groot::Repository::commit("change first", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    temporary.put("a.txt", "one\ntwo\nTHREE\n");
    groot::Repository::commit("change last", temporary.path);
    REQUIRE(groot::Repository::merge("feature", temporary.path).kind == groot::MergeResult::Kind::Merged);
    REQUIRE(groot::read_file(temporary.path / "a.txt") == "ONE\ntwo\nTHREE\n");

    groot::Repository::create_branch("conflict", temporary.path);
    groot::Repository::checkout("conflict", temporary.path);
    temporary.put("a.txt", "LEFT\ntwo\nTHREE\n");
    const auto other_tip = groot::Repository::commit("left", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    temporary.put("a.txt", "RIGHT\ntwo\nTHREE\n");
    groot::Repository::commit("right", temporary.path);
    const auto result = groot::Repository::merge("conflict", temporary.path);
    REQUIRE(result.kind == groot::MergeResult::Kind::Conflicts);
    REQUIRE(result.conflicts == std::vector<std::string>{"a.txt"});
    REQUIRE(groot::read_file(temporary.path / "a.txt").find("<<<<<<< HEAD") != std::string::npos);
    REQUIRE_THROWS(groot::Repository::commit("unresolved", temporary.path));
    groot::write_file(temporary.path / "a.txt", "RESOLVED\ntwo\nTHREE\n");
    groot::Repository::add("a.txt", temporary.path);
    groot::Repository::commit("resolved", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().merge_parent == other_tip);
    REQUIRE_FALSE(fs::exists(temporary.path / ".groot" / "MERGE_HEAD"));
}

TEST_CASE("merge abort restores the original branch and worktree", "[merge]") {
    HistoryDirectory temporary;
    temporary.put("a.txt", "base\n");
    groot::Repository::commit("base", temporary.path);
    groot::Repository::create_branch("other", temporary.path);
    groot::Repository::checkout("other", temporary.path);
    temporary.put("a.txt", "other\n");
    temporary.put("other.txt", "new\n");
    groot::Repository::commit("other edit", temporary.path);
    groot::Repository::checkout("main", temporary.path);
    temporary.put("a.txt", "main\n");
    const auto original_tip = groot::Repository::commit("main edit", temporary.path);
    REQUIRE(groot::Repository::merge("other", temporary.path).kind == groot::MergeResult::Kind::Conflicts);
    REQUIRE(fs::exists(temporary.path / "other.txt"));
    REQUIRE_THROWS(groot::Repository::checkout("other", temporary.path));
    groot::Repository::abort_merge(temporary.path);
    REQUIRE(groot::read_file(temporary.path / "a.txt") == "main\n");
    REQUIRE_FALSE(fs::exists(temporary.path / "other.txt"));
    REQUIRE(groot::Repository::log(temporary.path).front().hash == original_tip);
    REQUIRE_FALSE(groot::Repository::status(temporary.path).merging);
    REQUIRE_THROWS(groot::Repository::abort_merge(temporary.path));
}

TEST_CASE("diff reports empty file additions and deletions", "[diff]") {
    HistoryDirectory temporary;
    temporary.put("empty.txt", "");
    REQUIRE_FALSE(groot::Repository::diff(true, temporary.path).empty());
    groot::Repository::commit("empty file", temporary.path);
    fs::remove(temporary.path / "empty.txt");
    REQUIRE_FALSE(groot::Repository::diff(false, temporary.path).empty());
    groot::Repository::add("empty.txt", temporary.path);
    REQUIRE_FALSE(groot::Repository::diff(true, temporary.path).empty());
}

TEST_CASE("staging handles file and directory replacements", "[staging]") {
    HistoryDirectory temporary;
    temporary.put("folder/child.txt", "child");
    groot::Repository::commit("directory", temporary.path);
    fs::remove(temporary.path / "folder" / "child.txt");
    fs::remove(temporary.path / "folder");
    groot::write_file(temporary.path / "folder", "flat");
    REQUIRE(groot::Repository::add("folder", temporary.path) == 2);
    groot::Repository::commit("replace directory with file", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().files.size() == 1);
    REQUIRE(groot::Repository::status(temporary.path).deleted.empty());

    fs::remove(temporary.path / "folder");
    fs::create_directory(temporary.path / "folder");
    groot::write_file(temporary.path / "folder" / "child.txt", "child again");
    REQUIRE(groot::Repository::add("folder", temporary.path) == 2);
    groot::Repository::commit("replace file with directory", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().files.size() == 1);

    fs::remove(temporary.path / "folder" / "child.txt");
    fs::remove(temporary.path / "folder");
    groot::write_file(temporary.path / "folder", "flat again");
    groot::Repository::add("folder", temporary.path);
    groot::Repository::commit("flat again", temporary.path);
    fs::remove(temporary.path / "folder");
    fs::create_directory(temporary.path / "folder");
    groot::write_file(temporary.path / "folder" / "child.txt", "direct child");
    REQUIRE(groot::Repository::add("folder/child.txt", temporary.path) == 2);
    groot::Repository::commit("stage child directly", temporary.path);
    REQUIRE(groot::Repository::log(temporary.path).front().files.size() == 1);
}

TEST_CASE("merge recognizes ancestry despite shortcuts through merge parents", "[merge]") {
    HistoryDirectory temporary;
    temporary.put("seed", "base");
    groot::Repository::commit("base", temporary.path);
    groot::Repository::create_branch("shortcut", temporary.path);
    temporary.put("seed", "ours");
    groot::Repository::commit("ours", temporary.path);
    groot::Repository::create_branch("long", temporary.path);
    groot::Repository::checkout("long", temporary.path);
    for (int i = 0; i < 5; ++i) {
        temporary.put("seed", std::to_string(i));
        groot::Repository::commit("long " + std::to_string(i), temporary.path);
    }
    groot::Repository::checkout("shortcut", temporary.path);
    temporary.put("extra", "shortcut");
    groot::Repository::commit("shortcut work", temporary.path);
    groot::Repository::merge("long", temporary.path);
    const auto tip = groot::Repository::log(temporary.path).front().hash;
    groot::Repository::checkout("main", temporary.path);
    const auto result = groot::Repository::merge("shortcut", temporary.path);
    REQUIRE(result.kind == groot::MergeResult::Kind::FastForward);
    REQUIRE(result.hash == tip);
    REQUIRE(groot::Repository::merge("long", temporary.path).kind == groot::MergeResult::Kind::AlreadyUpToDate);
}
