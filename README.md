# Groot

Groot is a small, local version control system written in C++20. It lets you save versions of a folder, inspect earlier versions, work on branches, and combine branches. It is a learning project inspired by Git; it does not use Git internally.

This guide starts with the ideas behind the commands. You do not need to know Git or C++ to follow it.

## The idea in one minute

Imagine writing a document. You save a version today, change it tomorrow, and later want to see what it looked like today. Groot keeps those saved versions, called **commits**. A **branch** is a named line of commits, so you can try a change without moving your main line of work. A **merge** combines work from two branches.

Groot has three places to think about:

| Place | Meaning |
| --- | --- |
| Working folder | The files you see and edit in VS Code. |
| Staging area (the index) | The file versions you have chosen for the next commit. |
| Commit history | Snapshots you have already saved. |

The everyday cycle is **edit -> add -> commit**. Editing changes your working folder. `groot add` copies the current file content into Groot's object store and records it in the staging area. `groot commit` saves a snapshot of the staging area.

## What is a blob?

A **blob** is stored file content: the bytes inside a file. If `hello.txt` contains `hello`, its blob contains those five bytes. The file name is recorded separately. Groot stores blobs in `.groot/objects/` and names each one with a SHA-256 **hash**, a long string calculated from its content.

```text
hello.txt -> content: hello -> hash: 2cf24dba... -> .groot/objects/2cf24dba...
```

The example hash is for exactly `hello` without a newline. Commands such as `echo` or `Set-Content` usually add a newline, which produces a different hash. If two files have identical content, they can refer to the same stored blob. Changing one byte produces a different hash. Groot checks the hash when loading an object, so it can detect corrupted content.

A **commit** does not need to contain a fresh copy of every file. It records which file path points to which blob, along with a message, time, and parent commit. Each commit also has its own hash.

## What does `groot init` create?

Run `groot init` from the folder you want to track. It creates a hidden `.groot` directory there:

```text
your-project/
  your files...
  .groot/
    objects/          stored file content (blobs)
    commits/          saved snapshot records
    index             file versions staged for the next commit
    refs/heads/main   hash of the latest commit on main
    HEAD              the branch or historical commit you are viewing
```

`init` uses the terminal's current folder; it does not take a folder argument. The folder must already exist. Groot refuses to initialize a folder inside another Groot repository. You run `init` only once per project.

Groot and Git can exist in the same folder. Git uses `.git`; Groot uses `.groot`. If you use both, add `.groot/` to `.gitignore` so Git does not track Groot's data. Groot skips `.git` and `.groot` while scanning files.

## Save your first version

The examples below assume the `groot` command is available in your terminal. Start in a new folder outside any existing Groot repository:

```powershell
mkdir my-groot-demo
cd my-groot-demo
groot init

"Hello" | Set-Content note.txt
groot status
groot add note.txt
groot status
groot commit -m "Add my first note"
groot log
```

Here is what happened:

1. `groot init` created the repository metadata.
2. You created `note.txt` in the working folder. `status` reported it as **untracked** because Groot had not been asked to save it.
3. `groot add note.txt` stored its content as a blob and put `note.txt -> blob hash` in the index. `status` then reported a **staged** new file.
4. `groot commit -m ...` saved the index as a commit. The `main` branch now points to that commit.
5. `groot log` displayed the commit hash and message.

`groot add .` stages files under the current folder, including tracked deletions. `groot add <file>` stages one file. Running `add` again after editing a file updates the staged version. A commit uses the staged content, even if you make another unstaged edit afterward.

Useful checks while editing:

| Command | What it shows |
| --- | --- |
| `groot status` | Untracked files, staged changes, and tracked changes that are not staged. |
| `groot diff` | Changes between staged files and files in the working folder. |
| `groot diff --staged` | Changes between the current commit and the staging area. |
| `groot log` | Commit hashes and messages, newest first, along the current line of history. |

## Visit an older commit, then come back

`groot log` prints full commit hashes. Copy one and run:

```powershell
groot status
groot checkout <full-commit-hash>
groot status
groot checkout --latest
```

Replace `<full-commit-hash>` with an actual hash; do not type the angle brackets. The first checkout restores the older snapshot in your working folder. Groot calls this **detached HEAD**: you are looking at a commit directly, while your branch still points to its latest commit. `checkout --latest` returns to the branch you left and restores its current tip.

Checkout refuses staged or unstaged tracked changes and refuses to overwrite an untracked file. Run `groot status` first. Save work with `groot add` and `groot commit` before switching. Groot does not yet provide `stash` or `unstage` commands.

You cannot commit or merge while detached. To start new work from that older commit, create and check out a branch as shown next.

## Branch from an older commit

Suppose your history is:

```text
first -> second -> third -> fourth    main
```

To make a new line of work starting from `second`:

```powershell
groot checkout <hash-of-second-commit>
groot branch experiment
groot checkout experiment
```

`groot branch experiment` creates a **pointer** named `experiment` at the commit you are viewing; it does not duplicate the project folder. `groot checkout experiment` makes that branch active. Now edits and commits advance `experiment`, while `main` stays at `fourth`:

```text
first -> second -> third -> fourth    main
            \
             -> new work             experiment
```

Run `groot branch` to list branches. The `*` marks the active branch. You can also branch from the current latest commit: run `groot branch <name>` while on `main`, then `groot checkout <name>`. To return from an active branch, use `groot checkout main`; `--latest` is for the detached historical view.

## Merge a branch into `main`

A merge brings changes from another branch into the branch you are currently on. First finish and commit your work on both branches. Then switch to the destination branch:

```powershell
groot checkout main
groot merge experiment
groot status
```

Groot finds the most suitable shared ancestor of the two branch tips and compares three snapshots: the ancestor, `main`, and `experiment`. It combines changes made on only one side. If both sides changed different parts of a text file, Groot attempts to combine those edits too.

Two common outcomes:

| Situation | Result |
| --- | --- |
| `main` has not advanced since `experiment` branched | **Fast-forward:** `main` moves to the experiment commit. No new merge commit is needed. |
| Both branches have new commits | **Divergent merge:** Groot combines the snapshots and creates a merge commit with two parents. |

Merging does not delete the other branch. `groot log` follows the first parent of each commit, so a merged branch's individual commits may not appear in `main`'s log even though their changes are in the merge result.

## What if both branches change the same lines?

Groot cannot always decide which version you intended. For overlapping text changes, it leaves **conflict markers** in the affected file. The markers below are indented for display; in the file they begin at the start of each line:

```text
  <<<<<<< HEAD
text from your current branch
  =======
text from the branch being merged
  >>>>>>> experiment
```

`HEAD` here means the branch you were on when you ran `merge`. Open the file in VS Code, choose or combine the text you want, and remove the marker lines. Then save the file and finish the merge:

```powershell
groot status
groot add path/to/conflicted-file.txt
groot commit -m "Resolve merge conflict"
```

Stage every conflicted file before committing. Groot records the other branch in `.groot/MERGE_HEAD` during this process and removes that temporary merge state after the resolution commit. If you want to cancel the merge instead, run `groot merge --abort`; it restores the state from before the merge and discards conflict-resolution edits.

Binary conflicts and file-versus-directory path conflicts are rejected rather than merged automatically. A checkout or merge may also stop if an untracked file would be overwritten.

## Command reference

| Command | Purpose |
| --- | --- |
| `groot init` | Start tracking the current folder. |
| `groot add <file>` / `groot add .` | Stage file content or deletions. |
| `groot status` | See what changed and what is staged. |
| `groot diff` / `groot diff --staged` | Inspect working or staged changes. |
| `groot commit -m "message"` | Save a snapshot of staged files. |
| `groot log` | List commits on the current first-parent history. |
| `groot branch` / `groot branch <name>` | List branches or create one at the current commit. |
| `groot checkout <branch>` | Switch to a branch. |
| `groot checkout <full-commit-hash>` | View a saved snapshot directly. |
| `groot checkout --latest` | Return from a detached commit to the branch you left. |
| `groot merge <branch>` / `groot merge --abort` | Merge a branch or cancel a merge in progress. |

## Build and run

You need CMake 3.20 or later and a C++20 compiler. Catch2 is included in `third_party/`, so tests can run offline.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is normally `build/groot` or `build/groot.exe`, depending on your platform and build generator. Add its directory to your terminal's `PATH` to run `groot` from any project folder. On Windows with MinGW, make sure the matching compiler `bin` directory comes before incompatible MinGW runtime DLLs on `PATH`.

## Notes for curious developers

Groot stores file bytes under SHA-256 hashes in `.groot/objects/`. The sorted index maps repository-relative paths to those hashes. A commit stores a complete path-to-hash snapshot, its message and time, and one or two parent hashes. Its own serialized bytes are also SHA-256 hashed and stored in `.groot/commits/`. Branch refs store the commit hashes; `HEAD` identifies the active branch or a detached commit. Checkout updates the working files and index to match the target snapshot. Merge compares snapshots through their shared commit ancestry.

Groot is intentionally not Git-compatible. It has no remotes, push/pull, stash, reset, unstage, rename detection, binary merge, or garbage collection. It is intended for local, single-process use; concurrent writers and crash recovery are not guaranteed. Whole-file hashing and a quadratic text diff/merge algorithm make very large files less practical.
