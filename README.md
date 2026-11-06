# Groot

A lightweight Git-inspired version control system implemented from scratch in C++20. This repository currently contains **phases 1–4**: repository initialization, SHA-256 content storage, a persistent staging index, and immutable snapshot commits with history. Diffs, branches, and merges are planned phases.

## Why I Built It

Groot is an interview project for understanding how a version control system stores content and builds history. The implementation uses the filesystem directly and does not invoke Git or libgit2 for version-control operations.

## Current Architecture

```text
groot init -> Repository -> .groot/{objects,commits,refs/heads,HEAD,index}

groot add -> Repository -> ObjectStore -> SHA256 -> .groot/objects/<digest>
                       \-> Index       -> .groot/index

groot status -> compare Index with working files

groot commit -> snapshot Index -> .groot/commits/<digest> -> refs/heads/main

groot log -> follow commit parent hashes from the branch ref
```

`Repository::discover` walks from a directory to its parents looking for `.groot`. `Repository::init` creates an empty `main` branch, a symbolic `HEAD`, and an empty index. The object store takes arbitrary bytes, computes SHA-256, and stores the bytes under the digest. The same bytes always select the same object path, so identical content is stored once. Objects are published from a temporary file with a hard link; an existing hash path is never overwritten by `ObjectStore::store`. `load` verifies the digest and reports corruption.

```text
project/.groot/
├── objects/          # SHA-256-named immutable file contents
├── commits/          # SHA-256-named immutable snapshot records
├── refs/heads/main    # latest commit hash, empty until the first commit
├── HEAD               # ref: refs/heads/main
└── index              # sorted path -> object hash entries
```

SHA-256 is the *identity* of stored content, not a random identifier. For example, `hello` hashes to `2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824`. Two files containing those same five bytes can refer to one object. The implementation accepts binary bytes, including NUL bytes.

## Staging and Status

The index is a `std::map<path, hash>` serialized in sorted order to `.groot/index`. Each line contains a 64-character SHA-256 hash and a quoted repository-relative path. `groot add <file>` stores the file's current bytes as an immutable object and updates that path's hash in the index. It does not create a commit. `groot add .` recursively stages files under the current directory and removes missing indexed files in that subtree. Groot skips `.groot`, `.git`, and symbolic links while scanning.

`groot status` compares the latest commit with the index to report staged additions, modifications, and deletions. It also hashes indexed working files to find unstaged modifications, checks for missing indexed files, and scans for untracked files. A deleted file stays in the index until it is staged for removal with `groot add <file>` or `groot add .`.

## Commits and History

`groot commit -m "message"` captures the complete index as a snapshot. Each commit records its parent hash, timestamp, message, and sorted path-to-object mappings. The SHA-256 hash of that record names its file in `.groot/commits/`; the current branch ref points to the newest commit. `groot log` follows parent hashes from that ref, newest first. An unchanged index cannot create another commit. The working tree may have unstaged changes when committing; the snapshot uses only staged content.

## Build and Test

Requires a C++20 compiler and CMake 3.20 or later. Catch2 v2.13.10 is vendored in `third_party/` for offline tests.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

When running a MinGW build outside its IDE, put the compiler's `bin` directory on `PATH` so Windows can find its runtime DLLs. The tests create and remove their own directories in the system temporary directory. They do not initialize or modify this source repository.

## Try the CLI

Run these commands in a **new, empty directory** with the built `groot` executable on `PATH`:

```sh
mkdir demo
cd demo
groot init
echo hello > hello.txt
groot status
groot add hello.txt
groot status
groot add .
groot commit -m "Initial snapshot"
groot log
```

Inspect `.groot/HEAD`, `.groot/index`, `.groot/objects/`, `.groot/commits/`, and `.groot/refs/heads/main`. Running `groot init` a second time returns an error instead of replacing metadata. Run `groot status` after editing or deleting `hello.txt` to see the working-tree comparison. The old `.groot` directory in this source checkout uses the earlier prototype format; use a fresh directory for the demo.

## Algorithms and Costs So Far

| Operation | Time | Extra memory |
| --- | --- | --- |
| SHA-256 of *n* bytes | O(n) | O(n) for padded input |
| Store or load *n* bytes | O(n), plus filesystem access | O(n) |
| Discover at depth *d* | O(d) filesystem checks | O(1) apart from paths |
| Index lookup/update for *f* paths | O(log f) | O(f) for the loaded map |
| Commit of *f* indexed files | O(f + total staged file bytes), plus filesystem access | O(f + largest staged file) |
| Log over *c* commits | O(c + total snapshot entries) | O(c + total snapshot entries) |
| Status over *f* indexed files and *w* working files | O(f log f + w log f + total file bytes) | O(f + w) |

Hashing and storage operate on whole files for clarity. Large files will need streaming to reduce peak memory. Hard links must be supported by the repository filesystem. A production version would also add stronger crash recovery and concurrent-process testing.

## Next Phases

5. LCS line diff
6. Branch references and safe checkout
7. Commit DAG traversal and merge-base
8. Fast-forward and three-way merge with conflicts
9. Error handling, broader tests, and documentation polish

Groot is intentionally not Git-compatible. It has no remote synchronization, packfiles, rename detection, binary merge, or garbage collection.
