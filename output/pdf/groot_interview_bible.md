# GROOT
## The project interview bible

C++20 | content-addressed storage | staging and snapshots | graph history | diff and merge

Prepared from the working source in E:\GROOT and the CV project entry supplied in this conversation. Verification date: 11 November 2026, Asia/Calcutta. This guide describes the reviewed working tree, including fixes made during this review; it does not imply that every current feature existed during the CV's April-August 2026 period.

An evidence-based preparation resource: learn the system, explain the choices, defend the CV, handle follow-ups, and demonstrate the implementation.

# 01 | Start here: what to know cold

## The 90-second revision card

- Groot is a local, Git-inspired version control system implemented in C++20. It performs its version-control operations directly through C++ and the filesystem, without invoking Git or libgit2.
- Three states matter: the working tree is the editable directory; the index is the next staged snapshot; HEAD resolves through the active branch ref to the latest committed snapshot.
- File bytes are hashed with SHA-256. Identical bytes share one object regardless of filename. A commit stores a complete sorted path-to-object mapping plus parent links, a timestamp and a message.
- Branches are small reference files. A checkout restores the target snapshot and changes symbolic HEAD, after protecting tracked edits and blocking untracked collisions.
- Diff uses line-based longest common subsequence, or LCS. Same-file merge derives edits from a base snapshot and combines compatible edits.
- Commit history is a directed graph. A normal commit has one parent; a successful divergent merge has two. The log command follows only the first parent.
- Common-ancestor traversal follows both parent links. Ancestry is now checked by reachability before the distance heuristic chooses a base for divergent histories.
- Tested locally: 20 Catch2 cases, 148 assertions; one registered CTest entry; 41 independent CLI invocations; 16 byte samples cross-checked against Python hashlib.
- Main limits: local single-process design, nontransactional mutable metadata, whole-file memory use, quadratic LCS, simplified merge-base selection, no remotes, compression, packfiles, rename detection or garbage collection.
- Never promise 'lag free', universal automatic conflict resolution, Git format compatibility or production-grade durability.

## How to use this resource

First learn chapters 1-5 and rehearse the short introduction aloud. Then trace add, commit, checkout and merge in code. Use the topic questions as oral drills: answer for 45-90 seconds, point to the code, state a tradeoff, then stop. Practise the demo in a new directory. Finish with the tricky questions and the final mock interviews.

Answers written in first person are suggested interview language, not a verified record of your private motives or personal contribution. Adapt them to what you actually did. The review found and fixed bugs in November 2026; describe these as later review improvements unless they match your own historical work.

## Reading routes

- Two hours: revision card, CV audit, introductions, architecture, verification findings, tricky questions and demo.
- One day: all technical chapters, then perform two mock interviews without notes.
- Three days: rebuild and demo, hand-trace algorithms, implement one small improvement independently, then rehearse follow-up chains.

# 02 | Your CV: claim-by-claim defense

## The exact entry supplied

Groot Git-like Version Control System | C++20, STL, SHA-256, Graph Algorithms [Apr 2026 - Aug 2026]

- Created a Git-inspired version control system with 6 core operations including staging, commits, history, file diffing & branching.
- Implemented SHA-256 object storage for immutable file snapshots, enabling content deduplication and efficient version tracking.
- Designed 3-way merge and branching workflows with common-ancestor detection & auto conflict resolution handling support.
- Made entirely in C++20, leveraging trees, graphs and linked lists for high-performance and lag free version control operations.

## Claim 1: six core operations

The current CLI has nine command families: init, add, status, diff, commit, log, branch, checkout and merge. Branch has list/create forms; diff has working/staged forms; merge has an abort form. Your CV lists five categories after saying six. That may reflect an earlier scope, but the current source cannot establish that historical count.

Use a precise feature list instead of a fragile count. If asked about six, explain the categories you intended only if you remember them. Do not invent a historical grouping. Say: 'That bullet summarizes the initial core workflow; the current reviewed version also includes safe checkout and merge handling.' Verify the initial feature timeline against your own records before using that sentence.

### Q001. What are the six operations on your CV?

The wording is too compressed to reconstruct six unambiguously. The current implementation exposes initialization, staging, status, commits, history, diffing, branch management, checkout and merging. I would name the commands I can demonstrate and clarify that the CV summarizes feature categories. For future versions of the CV I would remove the number and list the supported workflows. A precise current feature inventory is stronger than trying to defend an arbitrary count.

### Q002. Is this a Git clone or a wrapper around Git?

It is a Git-inspired local VCS, not a wrapper and not a Git-compatible implementation. The CLI calls my Repository API, which operates on .groot metadata, immutable objects and commit files. The source does not execute Git for these operations. It implements a smaller model with a custom format. Git was used as an inspiration for the concepts; using Git to manage this source repository is separate from implementing Groot's own behavior.

## Claim 2: immutable snapshots and deduplication

Supported: ObjectStore hashes arbitrary bytes, names objects by the digest, publishes without replacing an existing hash path, verifies existing content before reuse and verifies the digest on load. A commit is a full manifest of paths referring to those objects. This deduplicates exact complete-file content; it does not deduplicate changed chunks or compress data. 'Immutable' describes the API publication rule, not an operating-system write-protection guarantee.

### Q003. What exactly becomes immutable?

Stored blob content and serialized commit records are treated as immutable by the API: they are published under content-derived names without overwriting an existing destination. Branch refs, HEAD and the index are mutable. Someone with filesystem write access can still tamper with the files; loading detects a digest mismatch. I would therefore describe immutable content-addressed records with integrity checking, not a tamper-proof storage system.

### Q004. How much storage does deduplication save?

I have verified the mechanism, not measured a representative savings percentage. Two paths containing exactly the same bytes use one blob, and unchanged content can be reused across many commits. Changing one byte in a large file stores another whole-file blob. Commit manifests themselves still consume space. I would benchmark workloads with repeated content and changing binary files before claiming a percentage or comparing it with Git's packed storage.

## Claim 3: three-way merge and automatic conflict handling

Supported: ancestor traversal through both parent links, fast-forward and up-to-date cases, path-level three-way decisions, combining non-overlapping line edits, conflict markers, staged resolution, two-parent commits and abort. Overlapping text edits require the user to choose a result. Binary conflicts and incompatible file/directory combinations can be rejected. The implementation cannot understand application semantics.

### Q005. Does 'auto conflict resolution' mean all conflicts are resolved automatically?

No. The accurate behavior is automatic merging of compatible changes plus explicit handling of conflicts. When both sides edit overlapping text, Groot writes conflict markers and records merge state. The user resolves and stages the result, then commits. Binary conflicts are rejected for manual handling. I would replace the phrase with 'automatic merging of non-overlapping edits, with conflict detection, staged resolution and abort support.'

### Q006. Why is the common ancestor necessary?

Comparing only the two branch tips shows that they differ but not which side changed a value. A base snapshot lets me distinguish one-sided changes from incompatible two-sided edits. If ours equals the base, I can take theirs; if theirs equals the base, I keep ours. Only genuinely two-sided changes need deeper merging. Without a base I would create avoidable conflicts or lose one side's change.

## Claim 4: trees, graphs, linked lists and performance

Supported: std::map and std::set provide ordered structures; std::deque is used for ancestor traversal; commit parent relationships form a directed graph. A first-parent history can be traversed like a linked chain. There is no std::list or custom linked-list container in the reviewed active source. The C++ standard guarantees ordered map behavior and logarithmic operations, not a mandated red-black implementation. Full-file hashing, repeated parsing and quadratic LCS contradict an unconditional 'lag free' claim.

### Q007. Where did you use trees, graphs and linked lists?

The index and snapshots use std::map for ordered key-value storage, with tree-like ordered-map behavior. Commit relationships form a graph with up to two outgoing parent links per commit. The first-parent log follows a linked chain of commit identifiers, but I did not implement a standalone linked-list container. Ancestor traversal uses std::deque as a queue. I would revise the CV to name these actual structures rather than imply a separate linked-list implementation.

### Q008. Is std::map necessarily a red-black tree?

No. Red-black trees are a common implementation, but the portable claim is the interface and complexity guarantees of an ordered associative container. My design needs unique sorted keys, deterministic traversal and logarithmic lookup/update. It does not depend on a specific balancing algorithm. If asked about red-black trees, I can explain the common implementation separately without claiming that I wrote one or that the language requires it.

### Q009. How did you prove the project is high-performance and lag free?

I cannot defend that absolute wording from the available evidence. The current review verifies correctness scenarios, not a performance SLA. Exact-content deduplication avoids redundant blob storage, but status hashes tracked files, commits verify staged blobs and LCS has quadratic cost. I would say the project explores efficient content reuse and explicit algorithmic tradeoffs, then propose measured latency, memory and storage benchmarks. 'Lag free' should be removed.

### Q010. Is the project entirely C++20?

The product CLI and core version-control implementation are C++20. CMake configures the build, Catch2 supplies tests, and this review adds a Python CLI verification script and document-generation tooling. Those supporting tools do not make the product core a Python application. I would phrase the claim as 'Implemented the VCS core and CLI in C++20 using the STL' and be transparent about build and testing dependencies.

## A more defensible CV entry

Groot - Git-inspired local version control system | C++20, STL, SHA-256, graph traversal

- Built a C++20 CLI supporting staging, snapshot commits, history, line diffs, branching and guarded checkout using a custom repository format.
- Implemented SHA-256 content-addressed blob storage with exact-content deduplication, digest verification and immutable object publication.
- Implemented three-way merges using ancestor traversal, automatic merging of non-overlapping text edits, conflict markers, staged resolution and abort support.
- Used ordered maps for deterministic snapshots and queue-based graph traversal for history analysis; validated the reviewed version with 20 test cases and an isolated CLI smoke workflow.

Use the final testing bullet only if you can explain and reproduce this review's checks. Keep April-August 2026 only if those dates reflect your actual project work; the current code and November verification do not prove feature completion dates. Do not attribute review-added fixes to the original period.

### Q011. Why did the wording change after review?

The new wording describes observable behavior and removes ambiguous implementation and performance claims. It gives an interviewer concrete things to test: whether identical bytes share an object, whether a commit uses staged content, and whether a conflicting merge can be aborted. Tightening a CV is an engineering improvement. I would acknowledge the earlier wording directly and then demonstrate the actual implementation rather than defend an exaggeration.

# 03 | Introductions, motivation and ownership

## A 30-second introduction

'Groot is a Git-inspired local version control system I built in C++20 to understand how version control works underneath the commands. It stores file content by SHA-256, stages a path-to-object snapshot, creates parent-linked commits and supports branches, checkout, diffs and three-way merges. The main learning was designing the boundaries between editable files, staged content and immutable history, then protecting those boundaries with validation and tests.'

## A two-minute introduction

'I wanted to understand what happens after commands such as add, commit and merge, so I implemented a small local VCS rather than calling Git. The architecture has a CLI, a Repository orchestration layer, an ordered index, an object store and hashing/diff modules. Adding a file stores its exact bytes under a SHA-256 digest and updates the index. A commit serializes the complete index with metadata and parent identifiers, then advances the active branch ref.

Branches are lightweight refs, so switching branches restores a snapshot rather than duplicating an entire repository. Merging uses a common ancestor: simple path changes are resolved by comparing hashes, and compatible edits in the same text file are combined using LCS-derived edit ranges. Overlaps enter an explicit conflict workflow with staged resolution and abort.

The difficult parts are data-loss protection, representing file absence separately from empty content, and reasoning about graph ancestry. In the latest review, additional tests exposed and fixed bugs in those areas. The reviewed version passes 20 test cases and 41 CLI commands. Its scope is educational and local: mutable metadata is not transactional, large diffs use quadratic memory, and it has no remotes or Git compatibility. My next priorities would be durability, stronger merge-base semantics and measured performance.'

Use the review details as 'latest review' language, not a fabricated account of your original development process.

### Q012. Why build a VCS when Git already exists?

The purpose was to learn systems design through a familiar workflow. A VCS combines persistent storage, content identity, graph traversal, deterministic serialization, text algorithms and filesystem safety. Implementing a limited version forced me to make those mechanisms explicit. I would continue to use Git for real collaborative work; the value of Groot is the engineering knowledge and the ability to explain the tradeoffs behind the commands.

### Q013. What problem does the project solve?

Within its local scope, Groot records selected file versions, preserves a sequence of snapshots, supports separate lines of development and combines compatible changes. It also solves a learning problem: commands that appear simple hide nontrivial state transitions. My design makes those transitions inspectable in a small .groot directory. I would avoid presenting it as a mature replacement for Git or as a team collaboration service.

### Q014. Why C++20?

C++ let me work directly with byte representations, filesystem operations, container costs and ownership. The implementation uses std::filesystem for paths, std::map for deterministic mappings, std::string_view for non-owning byte inputs, std::optional for meaningful absence, and std::rotr in SHA-256. C++20 also provides contains and starts_with used in the code. The tradeoff is more responsibility for resource use, platform behavior and error handling than a higher-level storage framework would require.

### Q015. What was your personal contribution?

Prepare a truthful inventory: which modules you wrote, which algorithms you adapted from references, which tests you authored, and where you used assistance. The source alone does not prove authorship or team boundaries. A strong answer names a module you understand deeply, walks through an invariant and discusses a bug you actually investigated. Do not claim Catch2, SHA-256 as a cryptographic invention, or review-added changes as solely original personal work.

### Q016. What was the hardest engineering challenge?

A defensible technical answer is preserving state while moving between snapshots. Checkout and merge must avoid overwriting local work, handle files becoming directories and reject invalid path combinations. Merge adds graph and conflict-state reasoning. Choose the challenge that matches your own experience and explain a concrete scenario, the invariant you wanted, and the test that demonstrates it. Do not substitute a dramatic story for evidence.

### Q017. What is the vision for Groot?

My near-term vision would be a reliable, inspectable educational VCS with documented behavior and reproducible tests. I would first strengthen local correctness and durability, then improve large-file and large-history performance. Remotes and collaborative workflows would come later because they introduce protocol design, authentication and consistency problems. A useful roadmap prioritizes dependable local state transitions before adding more visible commands.

### Q018. What would you change if you started again?

I would specify repository formats and invariants earlier, separate storage and worktree transitions more clearly, and design atomic updates for mutable metadata before broadening the CLI. I would also model file existence explicitly in diff APIs and treat ancestor reachability separately from selecting a merge base. These changes follow from real failure cases in the reviewed implementation, rather than a general wish to rewrite everything.

# 04 | Architecture and repository model

## Component map

```text
CLI: src/main.cpp
  -> Repository: command orchestration and invariants
      -> Index: sorted path -> blob digest
      -> ObjectStore: store/load immutable file bytes
          -> SHA256: deterministic content identity
      -> diff module: LCS diff and text edit merging
      -> utils: binary-safe file I/O
      -> filesystem: metadata, refs and working files
```

The core is a reusable C++ library, groot_core. The CLI and tests link against it. Tests can call Repository methods with an explicit directory without changing process-wide working directory. Most helpers in Repository.cpp have internal linkage through an anonymous namespace.

## On-disk layout

```text
worktree/
  files the user edits
  .groot/
    HEAD                 ref: refs/heads/main\n
    index                sorted digest + quoted path records
    refs/heads/main      tip digest + newline, or empty
    objects/<digest>     exact file bytes
    commits/<digest>     canonical snapshot + metadata
    MERGE_HEAD           other parent during a merge
    MERGE_CONFLICTS      paths requiring explicit staging
```

The source checkout also has an older .groot prototype. The README warns that its format predates the current implementation. Use a new directory for every demonstration; never repair or reinitialize the source checkout's .groot just to demo the current CLI.

## The three-state model

Let H be the committed snapshot at HEAD, I the index and W the working tree. Status reports H versus I as staged changes and I versus W as unstaged changes. The two comparisons are independent. Commit saves I even if W has newer edits. Checkout ordinarily requires H = I = W for tracked paths, while preserving unrelated untracked paths.

```text
working bytes -- add --> index mapping -- commit --> history
      W                    I                       H
         diff: I vs W          diff --staged: H vs I
```

Evidence: include/groot/repository.hpp; CMakeLists.txt; src/main.cpp; src/Repository.cpp functions discover, status, commit and diff; src/index.cpp; src/object_store.cpp. See source catalog in chapter 20.

### Q019. Why have a separate index?

It lets the user select a stable next snapshot independently of ongoing edits. After add stores version A, the user can edit the file to B; commit must still save A. Without a separate staged mapping, commit would have to read the current worktree and could include unintended changes. The index also makes staged deletions explicit: removing a path from the next snapshot is different from merely deleting a working file.

### Q020. Is the index a change list or a full snapshot?

It is the complete next snapshot, represented as a sorted path-to-hash map. An unchanged tracked file remains in the index after a commit. A staged deletion removes that path from the map. The staged-change list is computed by comparing the index with the committed manifest. This distinction matters: saving only recently added paths would accidentally drop unchanged tracked files from later commits.

### Q021. Why store full snapshots instead of patches?

A full manifest makes checkout and comparison straightforward: each path directly identifies the desired content. Unchanged files reuse existing blobs, so a full logical snapshot does not require copying all file bytes. The tradeoff is repeated path/hash metadata across commits. A patch-based representation can reduce metadata or storage for some workloads but complicates reconstruction, dependencies and corruption recovery. Groot prioritizes a clear model over optimal storage.

### Q022. What is HEAD and why is it symbolic?

HEAD contains the name of the active branch ref, not the latest commit digest itself. The branch ref contains the tip digest. This lets a commit advance the branch while HEAD continues to name the same branch. Checkout switches HEAD after restoring the target snapshot. The current implementation expects this symbolic form and does not support detached HEAD at an arbitrary commit.

### Q023. Why are blobs and commits in different directories?

They have different logical formats and operations. Blob objects are raw file bytes; commit files contain canonical metadata and a snapshot manifest. Separate namespaces simplify loading and validation and avoid confusing a raw blob with a commit record. Groot does not use Git's typed object-header format. A future unified object store would need explicit type and format-version separation before sharing one namespace.

### Q024. Why does Repository expose static methods?

It gives the CLI a small stateless-looking API, with each call discovering and loading repository state from a directory. Tests can supply temporary paths directly. The downside is repeated I/O and a large orchestration implementation. A longer-lived Repository object with injected storage and filesystem interfaces could cache validated state and make failure injection easier, but would need clear cache invalidation and concurrency rules.

### Q025. How does a command work from a subdirectory?

Repository::discover starts at an absolute path and walks upward until it finds .groot or reaches the filesystem root. require_root canonicalizes the discovered root. Added paths are stored relative to that root with generic separators. Thus adding a file from a nested directory still records src/file.cpp rather than a path relative to whichever directory the user happened to be in.

### Q026. Does finding .groot prove the repository is valid?

No. Discovery checks that .groot is a directory; it does not fully validate every expected file and object. Subsequent operations parse the index, HEAD, refs and commits as needed and can fail on malformed or old metadata. A production tool would benefit from a repository format version and a dedicated integrity checker. The source checkout's legacy .groot is a practical reason to keep discovery distinct from validation.

# 05 | Command walkthroughs and invariants

## init

init requires an existing directory and refuses to initialize inside a discovered repository. It creates objects, commits, refs/heads, a symbolic HEAD, an empty index and an empty main ref. An empty branch has no commit yet. Partial initialization after an I/O error is not transactionally rolled back.

## add

add discovers the root, rejects an empty request and a directly requested symlink, canonicalizes and validates scope, loads the index, stores selected file bytes and saves the resulting map. Directory staging skips metadata and symbolic links and removes missing indexed entries in scope. Staging now removes stale ancestor/descendant file entries when files and directories replace each other. The reported count is paths processed or removed, not necessarily paths whose bytes changed.

## commit

commit validates a one-line nonempty message, loads the index and current parent, checks merge resolution state if applicable, rejects unchanged ordinary snapshots, validates structural path compatibility and loads every indexed blob to verify integrity. It serializes a commit, publishes its immutable file, then writes the active branch ref. A merge commit can record unchanged content when two-parent ancestry itself is meaningful.

## checkout

checkout validates the target branch, returns immediately for the active name, requires tracked state to be clean for a real switch, loads both snapshots, preflights object loads and path collisions, applies the new worktree and index, then rewrites HEAD. Destination symlinks are rejected during preflight. This reduces expected overwrite risks; it is not an all-or-nothing transaction under disk failure or concurrent mutation.

## merge

merge requires a different existing branch and a clean tracked state. It handles empty/up-to-date and fast-forward cases first. For divergence it compares the union of base, ours and theirs paths. It may reuse a side's hash, remove a path, create a merged text blob, or record a conflict. Compatible results are applied; merge metadata and conflict markers are written; a conflict-free divergent merge is committed automatically. Blob creation before a later failure may leave unreachable objects.

### Q027. What invariants would you state in a design review?

Every stored digest has the expected syntax; a loaded blob or commit hashes to its filename; indexed paths are normalized repository-relative file paths; a committed snapshot cannot contain both a and a/b; snapshots are serialized deterministically; a commit references staged content; branch refs identify valid tip records when loaded; normal checkout protects tracked edits and untracked collisions. Some are enforced locally, but none implies global transactionality or hostile-filesystem safety.

### Q028. Why validate objects again during commit?

The index contains identifiers, not the actual bytes. A referenced blob might have been deleted or modified since staging. Loading and hashing every indexed object before publishing the commit detects missing or corrupt dependencies. This strengthens integrity but makes commit cost depend on total staged bytes, even if only one file changed. Caching verification or trusting immutable storage would require a stronger storage model and explicit invalidation rules.

### Q029. Can a file be both staged and unstaged?

Yes. Suppose HEAD contains A. I edit to B and add, then edit to C without adding again. The index contains B, so H versus I reports a staged modification; I versus W reports another unstaged modification. Commit saves B. This is a fundamental consequence of the three-state model and is tested in commit_test.cpp. It is not a contradictory status result.

### Q030. What happens when a tracked file is deleted?

Deleting it from disk initially leaves its index entry intact, so status reports an unstaged deletion. Running add for that missing indexed file, or add . over its scope, removes the entry from the index and stages a deletion. A subsequent commit omits the path from the new snapshot. The old blob remains reachable from older commits and is not physically deleted by this operation.

### Q031. What happens if add is interrupted after storing some blobs?

Some new immutable blobs may exist without a saved index update. They are unreachable data rather than necessarily lost previous history. However, index.save directly truncates and rewrites the index; an interruption during that write can damage mutable metadata. The current design lacks a transaction protocol and garbage collection. I would use a temporary index plus atomic replacement, with platform-aware durability and locking, before claiming crash safety.

### Q032. What happens if commit is interrupted?

If the commit file is published but the ref is not advanced, the old branch tip remains and the new commit may be unreachable. If the mutable ref write is interrupted, it may be incomplete because write_file truncates in place. The immutable publication step reduces partial-object exposure, but does not make the full commit transaction crash-safe. A lock, atomic ref replacement, recovery journal and durable flush ordering would be the next reliability work.

### Q033. Does checkout delete unrelated untracked files?

The intended behavior is to preserve unrelated untracked files and refuse collisions with target paths. apply_snapshot examines parent blockers and directories that would be replaced and removes paths from the old tracked snapshot when they disappear in the target. The existing tests cover important collisions and file/directory transitions. I would still avoid claiming exhaustive filesystem safety under all platforms, symlink races, permissions or external metadata manipulation.

### Q034. Why validate the entire target before writing files?

It catches expected failures such as a missing target blob or an untracked collision before destructive work begins. This is a preflight strategy. It does not guarantee rollback if a later write fails or the filesystem changes after validation. A transactional implementation would stage outputs, record intended mutations and have a recovery path. In an interview, the difference between preflight safety and atomicity is an important boundary.

# 06 | SHA-256 and content-addressed storage

## Hashing pipeline in the code

SHA256::hash accepts a string_view of bytes. It copies bytes into an unsigned-byte vector, appends 0x80, pads with zeros until the block position is 56 modulo 64, and appends the original bit length in eight big-endian bytes. It processes 64-byte blocks, expands sixteen input words into a sixty-four-word schedule, runs sixty-four rounds over eight 32-bit state words, accumulates state and prints sixty-four lowercase hex characters.

The algorithmic reference is NIST FIPS 180-4, especially sections 5 and 6.2. This is an educational implementation, not a cryptographic library certification. Its padding copy makes auxiliary memory O(n); the compression state itself is fixed size. Do not say the current whole function is constant-memory streaming.

## Publication pipeline

ObjectStore::store computes the digest and destination. If the destination exists, load verifies it. Otherwise the bytes are written to a temporary file in the objects directory. A hard link publishes the destination without replacing an existing path. The temporary name is then removed. If another destination already appeared, its digest is verified. Exception cleanup attempts to remove the temporary file.

Hard-link publication requires filesystem support. The counter and clock suffix reduce temporary-name clashes but do not establish a full multiprocess locking protocol or power-loss durability. Store and commit publication use similar patterns. The generic write_file helper itself is a truncating binary write, not atomic replacement.

Evidence: src/sha256.cpp; src/object_store.cpp; src/utils.cpp; tests/foundation_test.cpp; tools/interview_smoke.py. External reference: https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf

### Q035. Why SHA-256 instead of std::hash?

I need a stable content identifier across program runs and an extremely low practical chance of accidental collision. SHA-256 defines a fixed byte-to-digest mapping, unlike std::hash, which is designed for hash tables and does not promise a persistent cryptographic identity. The 256-bit output is represented as 64 hexadecimal characters. This choice supports deduplication and integrity checking, but hashing is not encryption or authentication.

### Q036. Why not assign random IDs to file versions?

Random IDs would name stored versions but would not directly identify equal content. I would need another lookup mechanism to deduplicate and separate checksums for integrity. With content addressing, identical bytes naturally select the same destination and a read can verify that the content matches the name. The tradeoff is hashing cost and the need to handle storage corruption and the theoretical possibility of collisions.

### Q037. What is a hash collision and how do you handle it?

A collision is two different byte strings with the same digest. The implementation assumes SHA-256 is suitable for practical content identity and verifies digest consistency on load. That verification catches ordinary corruption, but a real collision also passes a digest check. The code does not compare stored bytes against newly requested bytes after a matching digest or implement a collision-disambiguation format. I would state that limitation instead of claiming collisions are impossible.

### Q038. What does the birthday bound mean here?

For an ideal b-bit hash and k independently distributed objects, collision probability is approximately k(k-1) divided by 2^(b+1) when the probability is small. For b = 256, generic collision search is on the order of 2^128 operations; preimage search is a different property with a different generic cost. These are conceptual estimates, not measured guarantees for this project. Good hash choice also does not solve concurrent writes, durability or malicious ref replacement.

### Q039. Does hashing protect a repository from an attacker?

It detects a mismatch between bytes and an expected digest. It does not prove who authored a commit, encrypt files or stop an attacker from replacing a branch ref with another self-consistent history. Authenticity needs a trusted root such as verified signatures, and confidentiality needs encryption and key management. Groot has neither. I would use the phrase 'integrity checking of content-addressed records', not 'secure version control' without qualification.

### Q040. Why is unsigned arithmetic important in SHA-256?

The compression algorithm uses additions modulo 2^32. std::uint32_t provides the intended fixed-width unsigned arithmetic where available, and rotations and shifts operate on these word values. The implementation converts input bytes to unsigned values before assembling big-endian words, avoiding sign extension for bytes above 127. Signed overflow would be a correctness problem and must not be used to implement the modular arithmetic.

### Q041. What is the difference between a rotate and a shift?

A shift discards bits that move past an edge and fills the vacated positions according to the operation's rules. A rotation wraps the discarded bits around to the other side. SHA-256 combines rotations and logical shifts in its sigma functions; replacing a rotate with a shift changes the algorithm. The code uses C++20 std::rotr rather than manually composing rotations, which keeps intent clear and avoids several hand-written edge cases.

### Q042. Why does padding include the original length?

SHA-256 defines a precise block encoding that includes the original message bit length. This preserves the algorithm's required distinction between messages and their padding and yields a whole number of 512-bit blocks. In this code the original size is captured before appending padding. Boundary lengths around 55, 56, 63 and 64 bytes are especially useful tests because they change whether padding needs an extra block.

### Q043. Can std::string safely store binary content?

Yes, it stores a sequence of char values with an explicit size and can contain embedded NUL bytes. It becomes unsafe if code treats the buffer as a zero-terminated C string and ignores length. Groot uses string_view sizes, binary streams and ostream::write for file data. The tests include NUL and high-valued bytes, and the independent smoke workflow checks byte-for-byte storage against hashlib digests.

### Q044. Is the object store compressed or delta encoded?

No. Each new distinct whole-file byte sequence is stored as raw bytes. Exact duplicates share a digest; near duplicates do not share internal chunks. This makes storage and verification easy to reason about, but a large binary file changed slightly on each commit can consume substantial space. Compression, chunking, packfiles and deltas are possible later extensions, each with format, indexing and recovery costs.

### Q045. Is hard-link publication the same as atomic rename?

No. Here a temporary file and destination become names for the same file data, and creating the destination link does not replace an existing name. This fits the immutable publish-if-absent requirement. Renaming is a different operation, especially when replacing existing paths across platforms. Neither operation alone establishes that data and directory entries survive power loss, or that a sequence of index/ref/worktree changes is transactional.

### Q046. Why re-read an existing hash path?

An existing filename is not proof that its bytes are correct. The file may be truncated, corrupted or externally modified. load recomputes SHA-256 and rejects a mismatch, so store does not silently reuse known-bad content. This improves correctness at the cost of extra reads. It still assumes the digest is the identity and does not implement byte-level disambiguation for a true SHA-256 collision.

### Q047. What would a streaming SHA-256 implementation change?

It would maintain the eight-word digest state, total byte count and a small partial-block buffer across update calls, then pad only at finalization. Hashing could process file chunks without copying the entire file into memory. The object-store API and read_file use whole strings today, so replacing the hash alone would not eliminate all whole-file allocations. I would redesign the I/O pipeline as well and test chunk-boundary equivalence.

# 07 | Index, serialization, commits and history

## Canonical formats

The index writes records in std::map order: a 64-character digest, a space, a std::quoted path, then newline. Paths may contain spaces and quotes; line-breaking path names are rejected. The parser checks hashes and paths, trailing data and duplicate entries. Unlike commit loading, index loading does not enforce a complete serialize-and-compare canonical representation.

```text
parent <digest-or-dash>
merge-parent <digest>       [only for a merge commit]
time <epoch-seconds>
message "one-line message"
files
<blob-digest> "relative/path"
```

Commit loading verifies its filename digest, parses headers and entries, validates paths and hash syntax, and reserializes the result. Exact byte equality rejects a noncanonical record. The file manifest is flat; there are no Git-compatible tree objects, file mode entries or author identities. A timestamp and message affect commit identity even when content is otherwise identical.

### Q048. Why deterministic serialization?

The hash names the exact serialized bytes, so incidental iteration order or formatting changes would change identity. Sorted maps and a fixed format give the same logical record a stable byte representation. Determinism improves tests and makes inspection and corruption detection easier. The format still includes time and parents, so equal file snapshots alone do not imply equal commit digests. A future format change should be versioned explicitly.

### Q049. Why std::map instead of unordered_map for snapshots?

std::map maintains sorted unique paths, supports logarithmic lookup and update, and gives deterministic traversal for serialization. unordered_map offers average constant-time lookup but needs a separate sorting step for a stable serialized order and has different worst-case behavior. My choice favors clarity and determinism. It does not make all repository operations logarithmic, because most commands also scan files, parse records or hash bytes.

### Q050. What does std::quoted solve and what does it not solve?

It provides escaping for quoted string fields, allowing paths such as 'hello world.txt' to round-trip without splitting on whitespace. The code additionally rejects carriage returns and newlines in index paths and commit messages. Quoting alone does not prevent path traversal, invalid hashes, duplicate entries or platform-specific filename aliases. Those require separate validation. Do not describe std::quoted as a security sanitizer.

### Q051. Why validate a commit by reserializing it?

Parsing can accept representations that express the same values but differ in whitespace, ordering or optional quoting. Reserializing and comparing exact bytes ensures the record follows one canonical encoding. That removes ambiguity in a content-addressed metadata format. The hash check already ties bytes to a filename; canonical validation adds a structural format rule. The index parser is less strict and does not perform this final comparison.

### Q052. What fields affect a commit hash?

The normal parent, optional merge parent, timestamp, message and every sorted file path/hash entry affect serialized bytes and therefore the digest. The hash field stored in the in-memory Commit struct is not serialized into itself. Changing a commit message or parent changes the identifier even if file content is identical. SHA-256 names the record; it does not assign a sequential revision number.

### Q053. Why can a merge commit have the same files as its first parent?

Content identity and history identity are different. A two-parent commit can record that another branch has been integrated even when the resulting file map is unchanged, such as when both sides independently made the same change. Groot rejects an unchanged normal commit but permits a merge commit with meaningful parent relationships. Removing that allowance would make some legitimate graph transitions impossible to represent.

### Q054. Is commit history a linked list or a graph?

The complete history is a directed graph because a merge record can refer to two parents and many children can share a parent. Following only the first parent yields a chain, which is how log currently works. There is no in-memory linked-list container representing the repository. An interviewer asking about linked lists is likely testing whether the CV describes the actual data model or merely familiar terminology.

### Q055. Why does log not show every merged commit?

Repository::log follows only record.parent, so it provides the active first-parent sequence. A merge's second parent is stored and used by ancestor traversal but is not recursively displayed by log. A complete graph log would need traversal of both parents, deduplication and an ordering policy such as topological order. I would explicitly state that limitation rather than imply Git's graph visualization behavior.

### Q056. Why keep a seen set in log?

It detects a repeated identifier and avoids infinite traversal if malformed history contains a cycle. Content-derived parent-linked records make ordinary cycles difficult to construct, but defensive validation should not rely only on that assumption. A set adds membership checks during traversal. This does not imply that log validates every blob in every historical snapshot; it validates commit records as it loads them.

### Q057. Can two commits created in the same second collide?

Sharing a timestamp is not a hash collision. If all serialized fields, including parent, message and file mapping, are identical, the commit bytes and digest are intentionally identical. If any field differs, SHA-256 should produce a different digest with overwhelming practical probability. In normal sequential commits the parent changes anyway. Timestamp granularity affects metadata precision, not uniqueness by itself.

### Q058. What is missing from commit metadata?

There is no author or committer identity, timezone field, signature, explicit format version or file mode metadata. Paths map directly to raw content digests. These omissions simplify the educational model, but they limit attribution, executable-bit preservation, interoperability and future migrations. I would add format versioning before expanding the metadata, so readers can distinguish records rather than infer their structure heuristically.

# 08 | Diff: algorithm, edge cases and costs

## LCS recurrence

Each line retains its trailing newline when present. Let L[i,j] be the length of a longest common subsequence of the suffixes of the two line arrays. If left[i] equals right[j], L[i,j] = 1 + L[i+1,j+1]. Otherwise L[i,j] = max(L[i+1,j], L[i,j+1]). A bottom-up matrix supports deterministic reconstruction into unchanged, inserted and deleted lines. On a tie, the current code favors insertion.

line_diff produces one whole-file unified-style hunk, not a compact collection of context hunks and not a guaranteed Git patch format. NUL detection reports 'Binary files differ'. A missing final newline receives a marker. Repository::diff decides which paths to compare and now preserves empty-file existence changes that byte-only comparison cannot represent.

The matrix is capped at 10,000,000 cells through a division-based check before allocation. On a 64-bit platform a size_t matrix near that cap is about 80 MB, before line arrays and output. The cap is not a guarantee of low latency or low memory for the entire command.

Evidence: src/diff.cpp; Repository::diff; tests/history_test.cpp. The equations and costs here are derived from the reviewed code.

### Q059. Why use LCS for a text diff?

Common lines provide anchors that let me express the rest as insertions and deletions while preserving order. The dynamic-programming implementation is compact and easy to test on small files. Its main drawback is O(a*b) time and matrix space for a and b lines. I chose it for explanatory clarity; a production-oriented implementation would evaluate Myers, bounded-context output or other approaches on realistic workloads.

### Q060. What is the difference between a substring and a subsequence?

A substring must be contiguous. A subsequence preserves order but may skip elements. Diff needs common ordered lines even when edits occur between them, so a subsequence model is useful. If I required a single longest contiguous block, edits near the middle could hide useful matches elsewhere. LCS supplies multiple ordered anchors, although repeated lines can still create ambiguous alignments and unintuitive edit grouping.

### Q061. Why preserve newlines inside line tokens?

A file ending in 'hello' differs from one ending in 'hello\n'. Keeping newline information in each token lets the algorithm detect that distinction and lets output indicate the missing final newline. If splitting discarded all delimiters, those byte-distinct files could compare equal. The same principle appeared in the empty-file bug: a byte or token representation must preserve the semantic distinction the command promises to display.

### Q062. Why did the empty-file bug occur?

The low-level diff sees byte strings. A missing file was represented as an empty string, which is also the content of an existing empty file. line_diff returned no change for equal strings, hiding an addition or deletion. The fix keeps existence information at Repository::diff and emits structural empty-file metadata when path presence differs. The regression checks staged creation plus unstaged and staged deletion.

### Q063. Why not put the empty-file fix inside line_diff?

line_diff receives before bytes, after bytes and a display path; it does not know whether either file exists in a snapshot. Treating two empty strings as different would break valid equal-empty comparisons. Repository::diff knows which mappings and working paths exist, so it is the correct place to add existence semantics. A future API could pass explicit optional file states to make this boundary even clearer.

### Q064. How are binary files detected?

The current heuristic is the presence of a NUL byte. It avoids displaying arbitrary bytes for many binary files but does not classify every binary format correctly. A short binary sample with no NUL may be rendered as text. This is a known simplification, not a comprehensive file-type detector. The smoke tool decodes CLI output tolerantly so arbitrary output bytes do not break verification of unrelated behavior.

### Q065. Can Groot's diff be fed into git apply?

No compatibility guarantee exists. The output identifies itself with 'diff --groot', shows whole-file hunks and now has custom empty-file metadata. It is intended for inspection. There is no patch-apply command or test establishing Git patch compatibility. If compatibility became a requirement, I would specify a standard format, cover additions/deletions and quoting carefully, and test round-tripping with an independent parser.

### Q066. How would you reduce LCS memory?

If I only needed the LCS length, two rows would reduce memory to linear in one dimension, but diff reconstruction also needs a path. Hirschberg-style divide-and-conquer can reconstruct with linear auxiliary space at the cost of more complex control flow. Myers is another practical option for edit scripts. I would choose based on required output, input distributions and benchmarks rather than assume one algorithm dominates all workloads.

### Q067. Why cap the number of matrix cells instead of just lines?

The allocation depends on the product of the two dimensions. One very long side paired with a tiny side may be affordable, while two medium-sized inputs may exceed memory. The division-based check limits the product before multiplication, reducing overflow risk in that calculation. It remains a coarse resource guard: total output size, line copies and additional merge structures also consume memory.

### Q068. What is a non-obvious problem with repeated lines?

Several LCS alignments can have the same length, so a deterministic tie rule may choose anchors that do not match a human's intent. This can change how edits are grouped and whether two changes appear to overlap during merge. Correctly preserving bytes is not the same as finding the most intuitive diff. Repeated-line, insertion-boundary and newline cases deserve dedicated tests before claiming robust text-merge behavior.

# 09 | Branches, graph traversal and three-way merge

## Graph vocabulary

Commits point backward to parents. Branch refs are named roots for reaching history. A fast-forward moves a ref to a descendant without creating a new commit. An already-integrated target is an ancestor of the current tip. Divergence requires merging from a base. A merge commit has first parent = previous current tip and second parent = other branch tip.

```text
        B -- C       feature
       /
A ----+
       \
        D -- M       main
             ^
             second parent is C
```

The actual stored edges point from C to B, B to A, D to A, and M to D and C. The diagram is drawn in development order for readability.

## Path-level decision table

- If ours equals theirs, keep that shared result, including shared absence.
- If theirs equals base, keep ours.
- If ours equals base, take theirs, including a deletion.
- If both changed and base/ours/theirs all exist, attempt line merging.
- Otherwise, or if text edit ranges overlap, report a conflict unless binary/path constraints require rejecting the operation.

## Text merging

merge_lines computes base-to-ours and base-to-theirs edit ranges. Each edit replaces a half-open base-line range [begin,end) with a vector of new lines. Identical edits are deduplicated. Overlapping replacements, different insertions at the same position, and insertions strictly inside replaced ranges conflict. Compatible edits are sorted and applied to the base. This is syntactic merging, not semantic correctness checking.

## Common-ancestor implementation and an important limit

ancestor_distances uses a deque and ordered distance map, loading commit records and traversing both parents. merge_base now first checks whether either tip is reachable from the other, which correctly handles fast-forward and up-to-date decisions even with merge-parent shortcuts. For remaining divergent histories it chooses one common ancestor with minimum summed distance. That is a simplification: a minimum-distance common ancestor is not a general implementation of Git's best-common-ancestor relation, and multiple bases can exist. Complex divergent/criss-cross histories remain future work.

Git's own definition uses ancestry dominance rather than shortest distance. Reference: https://git-scm.com/docs/git-merge-base. This comparison is conceptual; Groot does not implement that command or Git's full merge strategies.

### Q069. Why use a graph rather than a tree for history?

Branching alone can be drawn as a tree-like structure, but merging allows a commit to have two parents and shared ancestry can be reached through multiple paths. That is a directed acyclic graph in the intended model. Tree-only traversal would miss merge-parent history and could choose the wrong ancestor. The code reflects the graph through two parent identifiers and a visited-distance structure, not a custom graph-library object.

### Q070. Why use BFS for ancestor distances?

With unweighted parent edges, breadth-first traversal finds the shortest number of parent steps to each ancestor. The deque supports FIFO processing, and the distance map suppresses repeated work when a known path is no worse. Both parent links must be visited. These distances support the current heuristic; they do not by themselves define the correct merge base for every graph. Reachability and base selection are distinct questions.

### Q071. Is the merge-base problem ordinary lowest common ancestor?

Not the tree version. A merge DAG can have several common ancestors with no single unique lowest candidate. Tree LCA algorithms assume one parent per node and different structural properties. Groot uses graph traversal and a simplified single-base selection. A stronger design would find common ancestors, remove those dominated by newer common ancestors and define what to do when several best bases remain, rather than use tree LCA terminology casually.

### Q072. What exactly was the ancestry bug found during review?

A merge parent can create a short route to an old ancestor. The previous algorithm selected the common ancestor with the smallest sum of distances before checking ancestry. That old node could score better than a tip that was actually an ancestor, causing a valid fast-forward to enter divergent merging and even conflict. The fix checks tip reachability first. The regression builds a long branch and a shortcut merge to reproduce the failure.

### Q073. Why does the fix not solve every merge-base problem?

It makes the special cases depend on the correct property: whether one tip reaches the other. For genuinely divergent histories, the code still uses minimum summed distances to choose one common ancestor. That heuristic can disagree with ancestry-based best-base semantics and cannot synthesize multiple bases. I would describe the fix accurately as correcting ancestry classification, then identify general divergent-base selection as a remaining limitation.

### Q074. Why does a fast-forward not create a commit?

The target already contains the current history, so there is no independent line of development to combine. Restoring the target snapshot and moving the current branch ref to its tip records the integrated state without adding a new node. HEAD remains on the current branch. The code returns FastForward and the target digest; it does not rewrite the target branch or manufacture a two-parent commit.

### Q075. What does 'already up to date' mean?

The other branch's tip is already reachable from the current tip, or the other branch has no commits. There is no new history to integrate. The two tip snapshots do not need to have identical files: the current branch may include additional later changes. The graph relation, not a byte equality check of the worktrees, decides this case.

### Q076. How are modify/delete and add/add conflicts handled?

If one side deletes while the other leaves the base unchanged, the deletion can be taken. If one side deletes while the other changes the file, that is a conflict. If both add the same path with identical bytes, hashes agree; different additions conflict because there is no original file for the same-file three-way line algorithm. Text conflicts get markers, while binary conflicts can be rejected before worktree application.

### Q077. Can changes in the same file merge automatically?

Yes, when both sides differ from the base but their base-relative line edits do not overlap under the implementation's interval rules. The regression suite changes the first line on one branch and the last line on the other, and the merged file contains both. 'Same file' is not itself a conflict. Equally, an automatic line merge does not prove the resulting code compiles or behaves correctly.

### Q078. Can non-overlapping text edits still break the application?

Yes. One branch can change a function's signature while another adds a call at a different location. The line edits may be disjoint, but the combined program can fail to compile or violate a business invariant. Groot only reasons about text and history. A successful merge should be followed by the project's own tests and build. Semantic merging would require language-aware analysis and still would not remove the need for validation.

### Q079. What happens if both branches make the same edit?

At path level, equal resulting hashes mean the result is already agreed upon. Within line merging, identical base-range edits with identical replacement lines are deduplicated rather than treated as overlapping conflicts. This matters when two branches independently apply the same correction. The graph can still require a merge commit to record integration even when content is unchanged relative to one parent.

### Q080. Why do simultaneous insertions at the same location conflict?

If the inserted sequences differ, the tool has no reliable basis for choosing their relative order. Arbitrarily concatenating them could produce an invalid or surprising result. Identical insertions can be deduplicated, while different insertions at one base position require a decision. The current algorithm has explicit same-position and insertion-inside-range rules; boundary behavior is an area for additional property and adversarial testing.

### Q081. How does conflict state survive separate CLI calls?

MERGE_HEAD records the other parent digest, and MERGE_CONFLICTS lists affected paths. The current branch ref remains at the original tip until completion. Markers appear in the working tree, and the index holds the automatic merge result plus retained side choices. A later commit loads that metadata and checks that conflicted files are staged consistently with the worktree, then records both parents and removes merge metadata.

### Q082. Does commit verify that all conflict markers are gone?

No. It checks staged/worktree agreement for listed conflicted paths, not the semantic quality of the chosen result or the absence of marker text. If I explicitly stage a file still containing markers, that can count as my resolution. This avoids treating legitimate marker-like text as inherently illegal, but it is easy to misuse. An optional warning could improve UX; it would be a heuristic rather than proof of correctness.

### Q083. How does merge --abort work?

It loads the original snapshot from the unchanged current branch tip, removes conflict-marker paths where safe, applies the original snapshot against the current index and restores original tracked bytes. It removes merge metadata afterward. The existing regression verifies restoration and removal of automatically introduced tracked files. Abort may discard edits made during resolution; it is not a general undo stack or a guarantee against all metadata tampering and filesystem races.

### Q084. Why reject binary conflicts instead of writing text markers?

Injecting text delimiters into arbitrary bytes can corrupt a binary format and provides no meaningful way to combine it. Groot can choose a one-sided binary change when hashes and base relationships make that unambiguous. When both sides disagree, it reports an error for manual handling. A future UX could offer explicit ours/theirs selection or external merge drivers, but it should not pretend to merge arbitrary binary semantics.

### Q085. What if there is no common ancestor?

The helper returns an empty base identifier, which snapshot_at interprets as an empty snapshot. The current logic can therefore attempt a merge against an empty base rather than expose a dedicated unrelated-history policy. That behavior deserves explicit specification and tests. I would not claim it matches Git's default unrelated-history handling. A production design should decide whether to reject, require a flag or support a documented empty-base merge.

### Q086. Why are slash-containing branch names rejected?

The branch implementation intentionally uses a flat refs/heads directory and a restricted portable name set. It rejects separators, leading hyphens/dots, trailing dots and Windows device names. This avoids traversal and several cross-platform filename hazards. Git permits a richer reference namespace, but Groot does not need to copy every rule. Restricted names are a scope choice, not evidence of full path portability.

### Q087. Does branch creation copy files or history?

No. It writes another ref containing the current tip digest, or an empty ref for an unborn branch. The branch shares existing commits and blobs. The data model makes branch creation lightweight, but its actual implementation still performs repository/ref validation and filesystem I/O. I would describe it as copying a reference rather than claiming a benchmarked constant-latency operation on every filesystem.

# 10 | C++20, STL and implementation decisions

## Language feature map

- std::filesystem::path carries paths; canonical and weakly_canonical support normalization and containment checks.
- std::string owns bytes; std::string_view borrows bytes for the duration of a call. Neither implies text-only content.
- std::optional represents 'not found' in discovery and 'cannot merge automatically' in text merging.
- std::map stores sorted snapshots and ancestor distances; std::set collects unique paths and visited history; std::deque supports FIFO traversal.
- std::vector stores contiguous padded bytes, line arrays, edit lists and the LCS matrix.
- std::uint32_t and std::rotr express SHA-256 word arithmetic. std::chrono supplies timestamps and temporary-name components.
- std::atomic counters help vary temporary names inside a process; they do not serialize a repository transaction.
- Anonymous namespaces keep helpers local to a translation unit. Headers expose a comparatively small API.

Evidence: include/groot/*.hpp and src/*.cpp. Do not claim a data structure merely because it is a familiar interview topic; tie it to an actual use in these files.

### Q088. What is RAII and where does Groot use it?

RAII binds resource lifetime to object lifetime. Streams close their resources when destroyed; standard containers release owned memory automatically; test temporary-directory helpers clean up in destructors. write_file also explicitly closes the stream so it can check final write/close failure before returning. RAII reduces manual cleanup burden, but it does not make a multi-file operation transactional or automatically undo filesystem mutations already completed.

### Q089. Why use string_view for content arguments?

It accepts a non-owning view without an extra argument copy and preserves an explicit byte length, including embedded NULs. The view is valid only while its source lives. SHA256::hash and ObjectStore::store consume it synchronously and do not retain it. The implementation still allocates internally, so passing string_view does not make hashing zero-copy or storing constant-memory. I would never return a view into a destroyed local string.

### Q090. Why return optional rather than an empty string from merge_lines?

A successful merge may legitimately produce an empty file, so an empty string cannot also mean failure. std::optional distinguishes a valid empty result from inability to merge automatically. The same reasoning explains the empty-file diff bug: absence and empty content are separate states. Typed representations help prevent sentinel values from accidentally collapsing distinct meanings.

### Q091. Why use exceptions instead of return codes everywhere?

Filesystem and parsing failures can occur deep in a call chain. Exceptions let the core report a detailed failure without manually threading error status through every return value. main catches std::exception, prints an error and returns 1; invalid CLI syntax returns 2. Expected successful variants such as FastForward or Conflicts use structured results. The design could improve further with richer typed errors rather than relying mainly on exception strings.

### Q092. What exception-safety guarantee does add provide?

It does not provide a full strong guarantee for all on-disk state. Blob publication is designed to avoid replacing existing objects, and temporary files are cleaned up on many failures. But new unreachable objects can remain and index.save rewrites mutable metadata directly. The in-memory map may never be saved if an earlier operation throws. I would describe guarantees per subsystem rather than label the whole command strongly exception-safe.

### Q093. Why use vector for the LCS matrix?

A flat vector stores cells contiguously and avoids many separately allocated rows. The implementation computes row offsets using width, making the matrix compact relative to a vector of vectors. It still allocates O(a*b) size_t cells, which is the dominant limitation. Correct indexing and preallocation guards matter more than assuming contiguous storage alone makes the algorithm fast enough for large files.

### Q094. Why use deque rather than list for BFS?

The traversal needs efficient push_back and pop_front, which deque supports without requiring a linked-list node allocation for each element. The code does not need stable iterators across arbitrary middle insertions. A std::queue adaptor over deque would also express FIFO intent. Most importantly, the actual code uses deque; I should not tell an interviewer that I implemented BFS with a linked list simply because that is one possible implementation.

### Q095. How does const improve the API?

Const methods such as ObjectStore::load promise not to change the object's own stored configuration, and Index::entries returns a const reference to avoid copying or arbitrary external mutation. That does not mean load has no I/O or that a const method cannot create a new file through an external resource. Const expresses C++ object mutation rules, not a complete purity or filesystem immutability guarantee.

### Q096. What does noexcept mean on entries()?

It promises that this accessor will not throw an exception. Returning a reference to the existing map is a simple operation that fits that contract. If an exception escaped a noexcept function, termination would result, so it should not be added blindly to filesystem or allocating operations. It is a correctness contract, not a general instruction to the compiler to make a function faster.

### Q097. Why is std::move used for constructor path arguments?

The constructors receive a path by value and move it into internal path construction, allowing ownership transfer from temporary or already-copied arguments. std::move is a cast enabling move operations; it does not itself move bytes and does not mean a value was necessarily expensive to copy. The actual benefit depends on the selected operations and types. This is a modest ownership detail, not the main performance story of the VCS.

### Q098. Why cast a char to unsigned char before std::toupper?

Character-classification and conversion functions require either EOF or a value representable as unsigned char. Passing a negative signed char can be invalid. The branch-name transformation uses an unsigned-char parameter before calling toupper. The allowed branch-name set is already ASCII, but correct casting is still a useful habit. Path encoding across platforms is a separate issue and is not solved by uppercasing ASCII device names.

### Q099. What makes the build actually require C++20?

The code uses features such as std::rotr, string starts_with and associative-container contains that are available in C++20. CMake sets CMAKE_CXX_STANDARD to 20, marks it required and disables compiler extensions. This avoids silently accepting an older standard or relying on an extension by accident. The reviewed build uses GCC 15.2.0 through the CLion MinGW toolchain, but no cross-platform compiler matrix was run.

### Q100. Is there multithreading in the product?

The reviewed CLI executes commands synchronously. Atomic counters are used for temporary naming, but there is no parallel hashing pipeline or repository-wide synchronization. Atomics protect only those counter operations. They do not make Index::save, branch updates or checkout safe with concurrent writers. A future threaded implementation would need both in-process synchronization and a separate protocol for independent CLI processes.

# 11 | Complexity and honest performance discussion

## Variables and cost model

Let F be indexed paths, W working files, B the total bytes read for a command, C reachable commits, S the total serialized commit/snapshot bytes loaded, and a,b the line counts of compared files. Let P represent path processing and filesystem overhead. Container costs and byte processing both matter; ignoring B makes several apparently 'constant' operations sound much cheaper than they are.

- Hash n bytes: O(n) time and O(n) auxiliary memory in this implementation's padding copy.
- Store/load n bytes: O(n) byte work plus filesystem overhead; existing-store reuse still re-reads and hashes content.
- In-memory map lookup/update: O(log F); add also loads and saves the entire index and hashes selected file bytes.
- Status: approximately O(F log F + W log F + B + P), including parsing/serialization-related structures and working-file traversal. Path lengths affect constants and comparison cost.
- Commit: reads/verifies all indexed blobs, builds and serializes a full manifest; approximately O(B + F log F + P) with loaded ordered structures, not merely O(changed files).
- First-parent log: loads every visited complete commit record and retains the result vector, so O(S + C log C) time for parsing/validation/seen checks, with snapshot structures and strings retained in memory.
- Diff: per file, O(a*b) matrix work and space plus line/output bytes; totals across compared files must be added.
- Text merge: two LCS computations plus pairwise overlap checks between the two edit lists; O(a*b + a*c + Eo*Et + E log E) at line-operation level, excluding string comparison/copy lengths.
- Ancestor search: graph traversal plus ordered-map membership and full commit loading/validation. A useful conservative description is O(C log C + S and parsing/map costs), not simply O(C) end to end.
- Checkout: loads target content into a map before mutation, so memory can approach total changed target bytes. Some directory blocker checks scan tracked paths repeatedly; worst-case path validation can exceed a simple O(F) summary.

These are source-derived estimates, not benchmark results. The README's table is a simplified teaching summary; the more detailed costs above include work visible in the actual implementation.

### Q101. What is the biggest performance bottleneck?

For large text comparisons, the quadratic LCS matrix is the clearest algorithmic limit. For many large tracked files, status and commit repeatedly read and hash bytes. For long histories, commit loading parses full snapshot manifests even when traversal needs only parent metadata. Which dominates depends on workload. I would profile before optimizing, using separate large-file, many-small-file, long-history and divergent-merge benchmarks.

### Q102. Is add O(log F)?

Only a single in-memory map update is O(log F). Repository::add loads the index, reads and hashes content, may scan directories and writes the complete index back. Staging a flat file also scans entries to remove stale descendants after the review fix. An accurate command-level answer includes I/O and total index work. Quoting only the container operation's complexity is a common interview trap.

### Q103. Why is commit not constant time after staging?

It validates every indexed object through load, which reads and hashes its bytes, and serializes the complete file manifest. This intentionally catches missing or corrupt dependencies before publishing history. Staging has already stored content, but commit does not simply trust all stored filenames. A faster design might use a stronger immutable backend, verification cache and incremental tree structure, each with additional correctness obligations.

### Q104. What benchmark plan would defend performance claims?

Measure median and tail latency, peak resident memory, bytes read/written and repository size for commands on controlled datasets. Vary file count, byte size, repeated-content ratio, line count, edit distance and history depth. Separate warm and cold filesystem-cache runs and record hardware, compiler, build type and filesystem. Compare only equivalent operations and outputs. Current test runtime is correctness evidence, not an end-user latency benchmark.

### Q105. How would you optimize status?

First measure hashing versus traversal and metadata parsing. A stat cache could use size and timestamps to avoid hashing unchanged files, but it needs careful invalidation and a fallback when metadata is unreliable. Parallel hashing may help on some storage and hurt on others. Persistent indexes and filesystem notifications add complexity. I would preserve a correctness-first slow path and test timestamp edge cases before trusting cached status.

### Q106. How would you optimize history traversal?

Separate lightweight commit headers and parent metadata from full snapshot loading, cache validated records within a command, and consider a graph index for repeated ancestry queries. The current loader reconstructs path maps and verifies canonical serialization for every visited commit. Any cache must define its trust boundary if files can change externally. I would optimize repeated parsing before introducing more complicated graph algorithms without measurement.

### Q107. Would unordered_map make Groot much faster?

Possibly in a measured hot lookup path, but it is not a universal improvement. Deterministic serialization would still require sorted output, and hashing, filesystem I/O and LCS may dominate command time. Ordered maps also simplify predictable iteration and worst-case lookup reasoning. I would benchmark the real workload and include sorting cost rather than cite average O(1) lookup as a complete argument.

### Q108. Would multithreaded hashing always help?

No. It depends on file sizes, CPU capacity, storage bandwidth, caching and per-file overhead. Parallel reads can reduce locality or overload slower disks. A safe implementation would bound concurrency and memory, keep index publication deterministic and handle failures without partial mutable-state updates. In-process speedup also says nothing about safe concurrent commands touching the same repository.

### Q109. Why are full manifests a scalability concern?

Every commit repeats path/hash metadata even if only one file changes. A history with many commits and many files therefore grows metadata roughly with the sum of snapshot sizes. Loading history also reconstructs those mappings. A persistent tree representation could share unchanged directory structures and make localized changes cheaper, but introduces another immutable object type and more traversal logic. Groot currently shares file bytes, not manifest subtrees.

### Q110. Is a smaller repository proof of a faster VCS?

No. Deduplication can reduce storage for repeated content, while extra verification or decompression can increase runtime. Different commands have different bottlenecks. A small repository can still incur slow quadratic diffs or many filesystem operations. I would separate storage efficiency, CPU cost, I/O cost and user-visible latency and report each with an appropriate workload instead of treating them as one metric.

# 12 | Reliability, security and platform boundaries

## Guarantees and their boundaries

The implementation validates hash syntax, parses metadata, rejects many invalid paths and branch names, skips symlinks during recursive scanning and protects many checkout collisions. It does not operate in a hardened hostile filesystem environment. Path validation followed by a later open is subject to time-of-check/time-of-use races. Metadata files and directories are not uniformly protected against externally introduced symlinks or junctions. Direct add canonicalization can also hide intermediate symlink aliases. Treat these as hardening work.

Mutable index, branch refs, HEAD and merge state use direct file writes. There is no repository lock, journal, reflog or general rollback transaction. Hard-link publication of immutable files is a narrower mechanism. A successful clean local test does not prove behavior under power failure, disk exhaustion, antivirus interference, permission changes or concurrent writers.

The source was reviewed and built on Windows/MinGW. The CMake source filename was corrected to src/Repository.cpp to match Git's tracked casing, avoiding a known case-sensitive-checkout mismatch. A Linux or macOS build was not executed; no claim of cross-platform certification follows from std::filesystem usage.

### Q111. How do you prevent path traversal?

Index validation rejects absolute/rooted paths, nonnormalized representations, parent components and metadata components such as .groot and .git. add also canonicalizes scope relative to the discovered root. These checks prevent many accidental and crafted escapes. They do not make the complete filesystem interaction race-free or handle every platform alias and reparse-point behavior. For hostile inputs I would use stronger handle-relative operations and a clearly defined metadata trust model.

### Q112. Why are lexical and canonical path checks different?

Lexical normalization simplifies components such as a/../b without consulting the filesystem. Canonicalization resolves actual filesystem structure, including symbolic links; weakly_canonical can handle missing suffixes. Both are useful, but canonicalization can hide the fact that a user supplied an intermediate symlink path. Containment, aliasing, existence and symlink policy are separate concerns. One path-normalization function is not a complete security boundary.

### Q113. What was the symlink hardening change in this review?

apply_snapshot now rejects a symbolic link at any target file destination during preflight. This covers a dangling destination link that an exists-only check could miss and avoids following that destination while writing content. It is a focused defensive change, not a claim that every symlink/junction path is handled. Dedicated platform-specific symlink tests were not executed as part of the reported suite; that verification gap remains explicit.

### Q114. Is checkout atomic?

No. It preloads required content and checks expected blockers before changing files, which reduces predictable partial failures. It then mutates multiple working files, the index and finally HEAD. A write failure or external race partway through can leave mixed state. To offer atomic-like recovery I would need a transaction plan, staged files, durable metadata and recovery logic, not just more preflight checks.

### Q115. What happens when two processes commit at once?

They can read the same old parent, publish separate commits and then race to write the same branch ref. The last update may hide the other commit, or interleaved writes may corrupt mutable metadata. Atomic temporary counters do not coordinate processes, and immutable object publication does not serialize ref updates. A repository lock plus compare-and-swap expectations and atomic replacement would be a starting point for a proper protocol.

### Q116. How would you implement a lock?

Acquire a lock file using an exclusive-create primitive before a mutating command, record enough ownership information for diagnostics and release it reliably. Define stale-lock recovery carefully; do not delete a lock merely because it is inconvenient. Decide whether readers can operate during writes and whether lock scope covers the entire worktree transition. A lock prevents cooperating writers from racing but does not itself ensure durability or protect against malicious external mutations.

### Q117. How would you make ref updates safer?

Write a new ref value to a temporary file in the same directory, validate the expected old tip while holding the lock, flush according to the desired durability contract and replace the destination through a platform-appropriate atomic operation. A reflog or journal could retain the old value for recovery. The exact semantics differ across operating systems and filesystems, so I would test the protocol rather than assume one rename pattern is universal.

### Q118. How would you design garbage collection?

Start from all branch refs and any in-progress merge roots, traverse both commit parents, mark reachable commits and their referenced blobs, then sweep unmarked objects after a safety policy. Coordinate with writers so newly created or temporarily unreferenced data is not deleted. A grace period or transaction roots may be needed. Groot currently has no GC; staged-but-uncommitted index objects must also be treated as roots to avoid deleting the next snapshot.

### Q119. How do you recover a deleted file?

If a branch still points to a snapshot containing it, checking out that branch can restore it, subject to clean-state rules. Older blobs and commit records remain on disk, but the CLI has no general checkout-by-commit, restore command or reflog. A manual inspection could locate an old blob, but I would not present that as a supported recovery UX. The roadmap should add a precise restore operation and tests.

### Q120. Does a commit preserve executable permissions and empty directories?

No. The manifest stores file paths and content digests, not file modes, ownership or directory entries. Empty directories have no tracked file path and are not represented. Checkout creates parent directories as needed and removes empty old parents where possible. This is sufficient for many educational examples but loses metadata that a more complete VCS may preserve.

### Q121. Are Windows path names completely portable?

No. Branch-name validation explicitly handles several Windows device names and trailing-dot problems, but general tracked paths are not a fully portable namespace. Case aliases, Unicode normalization, reserved names, drive semantics and reparse points need more policy and tests. Two logically distinct strings can identify the same physical path on some filesystems. I would document supported naming rules and reject ambiguous snapshots before claiming cross-platform fidelity.

### Q122. Why is digest verification not a complete fsck?

load checks a requested object's bytes against its digest, and commit loading validates that record's structure. There is no command that exhaustively traverses every root, checks all dependencies, reports unreachable data and diagnoses all metadata inconsistencies. A full integrity checker would deliberately inspect graph reachability, refs, manifests, blobs, merge state and structural path validity. Local checks are valuable but do not imply a repository-wide audit tool exists.

# 13 | What was verified and what changed

## Review environment and result

Verification used Windows, GCC 15.2.0 from the installed CLion MinGW toolchain, CMake 4.3.1 and Ninja. A separate Release configuration used -Wall -Wextra -Wpedantic. The project configured and compiled; the observed compiler output contained no warnings for project code. Catch2 v2.13.10 is vendored and CTest registers one executable-level test named groot_core.

The original working tree passed 17 cases with 135 assertions. Three new regression cases were first run against the original behavior and all three failed. After fixes, the reviewed code passed 20 cases with 148 assertions. CTest passed 1/1 registered test. The isolated CLI workflow passed 41 subprocess invocations, including expected failures. These numbers measure different layers; 1 CTest entry is not 1 assertion and 41 invocations are not 41 distinct independent requirements.

## Failure 1: empty files vanished from diff

Trigger: stage an empty file, or delete a tracked empty file. Before: missing and empty both became empty byte strings, so line_diff emitted nothing. After: Repository::diff emits structural creation/deletion information when existence differs. Regression: staged empty-file addition, unstaged deletion and staged deletion all produce nonempty output. Lesson: model absence explicitly instead of using a valid empty value as a sentinel.

## Failure 2: replacing a directory with a file left stale paths

Trigger: commit folder/child.txt, remove the directory on disk, create a regular file called folder, and add folder directly. Before: add inserted folder but retained folder/child.txt, creating an impossible snapshot shape. After: staging removes incompatible indexed ancestors/descendants and reports both processed and removed paths. The same regression also checks file-to-directory replacement through directory and direct-child staging. commit also validates incompatible file/path prefixes before publication. Lesson: index structure must match realizable filesystem structure, not merely individually valid strings.

## Failure 3: ancestry was confused by shortcut paths

Trigger: a long descendant chain is merged into another branch with a shorter route to old history; then merge that tip into one of its ancestors. Before: minimum summed distance selected an older ancestor and classified the operation as divergent, sometimes producing a conflict. After: reachability decides fast-forward and up-to-date cases before the divergent-base heuristic. Regression: a constructed shortcut graph fast-forwards and a previously integrated ancestor reports up to date. Lesson: define the graph property before choosing an optimization metric.

## Additional hardening and build correction

Target destination symlinks are rejected during snapshot preflight, including dangling links. This particular hardening path was code-reviewed but did not receive platform symlink execution coverage in the reported suite. CMake now names src/Repository.cpp with the same case as the Git-tracked path. The source's physical filename was aligned without changing Git history or committing the user's existing work. Case-sensitive-platform execution remains unverified.

## Coverage map

- Foundation: init/discovery, duplicate/nested init rejection, standard SHA-256 vectors including one million 'a' bytes, byte round-trip, exact deduplication, invalid/missing hashes, corrupt object reads and reuse.
- Staging: ordered index persistence, spaces in paths, replacing staged hashes, recursive add, metadata exclusion, nested invocation, deletion staging, outside/missing/metadata path rejection.
- Commits: index-only snapshots despite newer worktree edits, parent chain, unchanged commit refusal, deletion snapshots, invalid messages, corrupt commit rejection.
- History workflows: working/staged diffs, binary diff notice, checkout collision protection, file/directory transitions, branch validation, fast-forward/up-to-date, two-parent merge, disjoint same-file edits, conflicts, staged resolution and abort.
- New regressions: empty-file diff semantics, directory-to-file staging and ancestry with merge-parent shortcuts.
- Independent CLI: command validation, 16 SHA-256 boundary/random byte samples versus hashlib, duplicate byte reuse, conflict/abort/resolution, corrupt dependency refusal and expected clean states.

## What this review did not prove

No concurrency stress, power-failure injection, disk-full recovery, exhaustive symlink/junction matrix, performance benchmark, fuzzing campaign, coverage percentage or Linux/macOS execution was performed. No remote workflow exists to test. The legacy .groot in the source checkout was not migrated. 'Works' here means the reviewed local build and listed behavioral checks pass, with known limitations documented.

### Q123. Why add tests when the original suite passed?

Passing tests shows agreement with covered expectations, not absence of bugs. I read the state transitions and looked for missing distinctions: empty versus absent, file versus directory and reachability versus shortest-path distance. Those cases were not covered by the original suite and exposed real failures. The new tests are meaningful because they failed before the change and passed afterward, rather than simply restating implementation details.

### Q124. How do you isolate tests from the real repository?

The C++ fixtures create fresh directories under the system temporary location and remove their own directories in destructors. Repository methods accept an explicit current directory, so tests do not need to change the source checkout's state. The Python smoke tool uses TemporaryDirectory and runs the CLI with that directory as cwd. This is important because the checkout already contains legacy .groot metadata.

### Q125. Why test both the core API and the CLI?

Core tests inspect structured results and metadata precisely, making algorithm and state assertions convenient. CLI tests exercise argument parsing, exit codes, process execution, output and default-directory behavior. Either layer alone misses failures at the other boundary. The independent hashlib comparison also reduces the risk of validating an incorrect hash implementation only against itself. The layers complement each other without proving complete coverage.

### Q126. Why is a hash round-trip not enough to test SHA-256?

An incorrect deterministic hash could store and load its own objects consistently. Known standard vectors and an independent implementation test whether the digest is actually SHA-256. The suite checks published familiar vectors and a million-byte case; the smoke workflow cross-checks sixteen byte lengths with Python hashlib, including padding boundaries. This is stronger than self-consistency, though not a certification of the implementation.

### Q127. Why enable compiler warnings in a separate Release build?

A clean configuration confirms that the source compiles independently of the existing build outputs, and warnings can catch suspicious conversions, unused values and portability issues. Release mode also exercises optimization, though this is still the same compiler/platform. No observed warnings is useful evidence, not proof of memory safety or undefined-behavior freedom. Sanitizers and a compiler/platform matrix remain useful future checks.

### Q128. What would you test next?

I would prioritize high-impact unproven boundaries: concurrent ref/index writers, partial mutable-file writes, symlink/junction collisions, malformed metadata and complex divergent merge bases. Property tests could check merge identity, preservation of disjoint edits and content-addressing round-trips. Failure injection around each write would reveal recovery gaps. Performance tests should be a separate suite with controlled datasets and metrics, not correctness tests with arbitrary timing thresholds.

# 14 | System-design extensions and tradeoffs

## A practical priority order

Priority 1: specify format versions and invariants, add locks and atomic mutable-file replacement, design recovery and add failure injection. Priority 2: strengthen general merge-base selection, path alias policy, integrity checking and restore UX. Priority 3: streaming I/O/hashing, better diff scalability and cached parent metadata, driven by measurements. Priority 4: compression, persistent trees, packing and safe garbage collection. Priority 5: remotes and collaboration after local consistency is dependable.

This is a proposed roadmap, not implemented behavior. Every additional feature creates new state, compatibility and recovery obligations.

### Q129. How would you add remote synchronization?

Define a protocol that advertises refs, determines missing reachable objects, transfers and validates immutable records, then updates refs under policy and expected-old-value checks. Handle authentication, authorization, transport security, interrupted transfers and non-fast-forward updates explicitly. Content addressing helps identify reusable data but does not provide the protocol itself. I would not start by copying the .groot directory over a socket and call that a reliable remote VCS.

### Q130. How would you implement rename detection?

Snapshots currently record paths and content, not rename events. A comparison layer can pair a deletion and addition with the same digest as an exact-content rename candidate; similarity-based detection needs additional heuristics and thresholds. Ambiguity arises when several files share identical bytes. Detection should not change immutable blob identity, and merge behavior needs its own rename-aware rules. This is currently absent, so I would present it as an extension.

### Q131. How would you add a restore command?

Define whether restore changes the worktree, index or both, and require an explicit source snapshot when needed. Reuse validated object loading and collision checks, but keep semantics distinct from branch switching. Protect local edits unless the user selects a documented overwrite option. Tests should cover missing paths, empty files, binary content, staged versus unstaged states and file/directory transitions. Clear state semantics matter more than merely exposing another command name.

### Q132. How would you add rebase?

Rebase replays changes onto a new base and creates new commit identities because parent relationships change. It needs an ordered replay plan, conflict state, continue/abort controls and protection against overwriting valuable refs. Groot has no cherry-pick or replay engine today. I would first define patch application and recovery behavior and distinguish rewriting history from merging history. It is a substantial feature rather than a small variant of checkout.

### Q133. How would you add tags?

Lightweight tags could be immutable or deliberately controlled named refs to specific commits in a separate namespace. Annotated tags would need another typed record with metadata, possibly signatures. Validation, listing, deletion policy and GC roots must be updated. The current branch machinery is conceptually reusable but should not make tags advance on ordinary commits. A named pointer is simple; durable and unambiguous lifecycle semantics still require design.

### Q134. How would you support file permissions and symlinks?

Extend the manifest entry type to distinguish regular files, symlinks and relevant mode bits, and version the format. Store a symlink target as data rather than following it during add or restore. Define platform capability and portability policy, especially when a target is unsafe or unsupported. Tests must verify that restoring one entry type cannot accidentally follow or overwrite an external target. This is a format and safety change, not just a filesystem copy option.

### Q135. Why not put all metadata in SQLite?

A database could provide transactions, indexing and stronger mutable-metadata update patterns, reducing some custom recovery work. It would add a dependency and change the educational goal of directly inspecting files and refs. Blob storage might still remain content-addressed outside the database, which creates cross-store transaction questions. I would choose based on reliability and product requirements, not assume a custom file format is inherently superior.

### Q136. Would compression change the hash identity?

It depends on the format contract. If identity remains SHA-256 of original bytes, compressed storage can be an encoding detail as long as load decompresses and verifies the original content. Hashing compressed bytes instead would make identity depend on compression choices and parameters. Groot currently hashes and stores raw bytes. A compressed format needs explicit version/type metadata and careful handling of corrupted or oversized decompressed data.

### Q137. Why add repository format versioning early?

Readers need a reliable way to know which encoding and rules apply before interpreting bytes. Groot's legacy source-checkout metadata demonstrates how old layouts can become incompatible with newer commands. A format marker enables deliberate migration or a clear refusal instead of confusing parse errors. Versioning also matters before adding file modes, typed objects, compression or new commit metadata that changes serialization and identity.

### Q138. How would you add CI?

Run configure, build and CTest on clean checkouts across the supported compiler/platform matrix. Include the Python CLI workflow, check source filename casing and run sanitizer builds where supported. Keep performance measurements separately reported so noisy shared runners do not create flaky correctness failures. CI should reproduce the documented local commands and preserve useful failure output. The current reviewed repository has no CI configuration in the active file inventory.

# 15 | Tricky questions and depth checks

These questions deliberately probe overclaims and boundary conditions. State the exact current behavior first, then propose an improvement. Do not silently answer as if a proposed future design already exists.

### Q139. If blobs are immutable, why can a corruption test overwrite one?

Immutability is a rule of the ObjectStore API, not an operating-system guarantee. The test deliberately bypasses that API through direct file writing, then verifies that load detects the inconsistency. A filesystem user can modify the stored bytes. Stronger protections would require permissions, trust boundaries or a more controlled backend, and even then recovery policy would be separate from detection.

### Q140. Does exists(hash) validate the content?

No. ObjectStore::exists validates the digest syntax and checks whether a regular file exists at that path. load performs content verification. Confusing existence with integrity would let corrupted objects be treated as safe. The store reuse path intentionally calls load rather than trusting exists alone. The distinction is small in the API but important for explaining the integrity model.

### Q141. Can an index be valid while its blobs are missing?

Yes. Index parsing validates the shape of paths and digest strings, not the presence or integrity of every referenced object. commit loads all referenced blobs and fails if a dependency is missing or corrupt. Status mainly compares mappings and working hashes, so a clean-looking status is not a full integrity guarantee. A future fsck should explicitly validate the whole dependency graph.

### Q142. Can a commit record load while one of its blobs is missing?

Yes. load_commit verifies the record's digest, structure and canonical form, but it does not load every file blob. Checkout loads required changed target blobs, and commit verifies indexed blobs. A log can therefore read a structurally valid record even when some underlying file content is unavailable. This layered validation avoids unnecessary byte reads but means the phrase 'valid commit' needs a precise definition.

### Q143. What if an unchanged target blob is corrupt during checkout?

apply_snapshot preloads blobs whose path/hash differs between snapshots, not every unchanged blob. If working content is clean and the digest is unchanged, checkout may leave that file in place without reading the corrupt stored object. This does not repair the object store. A full verification or later commit would be needed to detect that dependency corruption. The behavior is another reason to separate worktree correctness from complete repository integrity.

### Q144. Does checkout to the active branch require a clean tree?

No. The code returns early when the requested name is the current branch, before require_clean. It performs no snapshot switch in that case. A switch to another branch requires clean tracked state and refuses a merge in progress. This is a useful example of reading control flow precisely rather than making an overly broad statement that every checkout invocation enforces the same preconditions.

### Q145. Does merge return a nonzero exit code for text conflicts?

The current CLI prints conflict messages from a structured Conflicts result and then returns 0. Exceptions return 1 and invalid usage returns 2. A script that treats exit 0 as proof of a completed merge would be wrong. A better CLI contract might use a documented nonzero code for unresolved conflicts, with tests. This was identified by code review and is not changed in this review's fixes.

### Q146. Can I stage conflict markers and commit them?

Yes, if the listed conflicted path's working bytes match its staged digest, the current commit check accepts that explicit staged choice. It does not parse or reject marker strings. That design cannot infer semantic intent. I should inspect the resolved file before staging and run application tests afterward. An optional warning about remaining marker patterns would be useful but should be described as a warning, not a proof of unresolved state.

### Q147. Does add . mean 'stage only files changed since the last commit'?

No. It recursively processes regular files in scope and stores/reuses their content, including unchanged files, while removing missing indexed entries. The output count can include unchanged paths. Hash identity prevents duplicate blob storage, but the command can still perform repeated reads and verification. This explains why the count is not a change count and why add . can cost more than the number of modifications suggests.

### Q148. What if I remove an entire tracked directory and add that missing directory name?

The missing-path branch of add only removes an exact indexed path. A directory itself is not indexed, so adding the now-missing directory name can fail rather than stage every descendant deletion. Running add . from an existing parent scans the index scope and removes missing descendants. I would demonstrate that supported route and identify recursive missing-directory staging as an explicit future UX improvement.

### Q149. Does .gitignore control Groot's staging?

No. Groot skips the .git and .groot metadata components, but it does not implement .gitignore patterns or a .grootignore system. Running add . in the source repository could stage build outputs and generated documents if they are ordinary files. That is another reason to use a small fresh demo directory. A future ignore engine needs documented matching rules and tests, not merely skipping a filename called .gitignore.

### Q150. Why is the project not compatible with a Git object hash?

Groot hashes raw file bytes for blobs and uses a custom commit format. Git's object model includes typed objects and encoding rules, so even matching file content should not be assumed to have a matching object ID. Groot also lacks Git tree objects, refs semantics and pack formats. The conceptual inspiration is shared, but interoperability would require implementing and testing actual Git formats. Reference: the official Git Objects chapter listed in chapter 20.

### Q151. Could a normal commit with an empty file set be valid?

The initial empty-index commit is rejected. But after a nonempty committed snapshot, staging deletion of every path creates a changed empty snapshot, which can be committed. The distinction is not 'empty snapshots are always illegal'; it is whether an ordinary commit changes the previous snapshot, with a separate initial-empty check. The commit tests cover deleting a file and reaching an empty latest manifest.

### Q152. Why not delete old blobs when a new version is staged?

Older commits or other index paths may still reference those blobs. Removing them eagerly could destroy history or break deduplicated content shared by another file. Reachability from refs, the index and merge state determines whether data is safely collectible. Without a GC implementation, Groot keeps old objects. This is intentional retention, though unreachable data can accumulate after failed or abandoned operations.

### Q153. Does identical file content imply an identical commit?

No. A commit also encodes paths, parents, time and message. Two branches may have the same file content but different history. Two files may share a blob while appearing under different names in a manifest. The levels of identity are separate: blob = exact bytes; snapshot = path-to-blob mapping; commit = snapshot plus history metadata. Mixing these is a common source of incorrect deduplication and merge explanations.

### Q154. Can a graph be a DAG merely because timestamps increase?

No. Parent links define the graph structure; timestamps can be equal, wrong or move backward with clock changes. Groot uses identifiers and traversal state rather than relying on timestamp order for ancestry. A well-formed append-only construction naturally points new records to existing records, but defensive readers still validate syntax and guard repeated history nodes. Timestamp sorting is not a substitute for topological reasoning.

### Q155. Does the use of atomic counters make object storage lock-free?

No. The counters only generate distinct sequence values within a process. Object storage uses filesystem operations and can block, and the repository has no lock-free concurrency algorithm. Even immutable publication has temporary-name and platform concerns across processes. I would avoid applying concurrency terminology based on the presence of std::atomic alone and instead explain exactly what state the counter protects.

### Q156. Does a successful merge guarantee no data loss?

It demonstrates that the implementation selected or combined bytes according to its rules in that execution. It does not prove application semantics, crash recovery or resistance to concurrent mutation. Non-overlapping code changes can still be incompatible, and mutable metadata writes are not transactional. The practical workflow is to review the result and run the project's tests. Broad guarantees need a much stronger specification and failure model.

### Q157. If one side has an empty file and the other deletes it, are they the same?

No. The path-level mapping distinguishes an existing empty blob from no path. Merge uses missing hashes as absence and a valid SHA-256 digest for the empty blob, so those identities are distinct. Diff previously lost this distinction after converting both states to empty byte strings; the review fix preserves it at the repository layer. This example connects data modeling directly to user-visible correctness.

### Q158. What if a snapshot contains both a and a/b?

No ordinary filesystem can realize both as regular files at once. Individual path validation is insufficient because each path can be valid by itself. validate_snapshot_paths checks prefix conflicts, and commit now applies that validation to the index before publication. The add-file fix removes stale descendants when a tracked directory becomes a file. Cross-entry invariants belong alongside per-entry validation.

### Q159. Is the merge-base traversal O(V+E)?

That is the abstract cost of BFS with constant-time adjacency access and membership. The actual implementation uses ordered maps and loads, hashes, parses and reserializes full commit records. Those costs depend on graph size and snapshot bytes, with logarithmic map operations. I would state the conceptual traversal bound, then add the real storage and container costs. This demonstrates understanding rather than merely reciting a textbook bound.

### Q160. Why can a build succeed on Windows and fail on Linux?

Windows commonly uses case-insensitive path lookup, while a Linux filesystem often distinguishes Repository.cpp from repository.cpp. Git tracked the capitalized name while CMake originally referenced lowercase. The review aligned CMake and the physical source name with the tracked case. Toolchain availability, hard-link support and runtime behavior are additional portability questions. A successful Windows build still does not establish a passing Linux build.

# 16 | Behavioral answers and engineering judgment

Use the STAR structure only with events you genuinely experienced: situation, task, action and result. The following answers are technical scaffolds. The current review is a documented event; it is not proof that you personally diagnosed these bugs during the original development period.

### Q161. Tell me about a bug that taught you something.

A concrete example from the latest review is the empty-file diff bug. The representation used an empty string for both missing content and an existing empty file, so the diff silently lost additions and deletions. The fix kept existence information at the repository boundary, and tests covered staged creation and both deletion states. The lesson is to represent distinct domain states explicitly. Attribute the investigation accurately if you discuss assistance used during review.

### Q162. How do you choose correctness versus performance?

For a storage tool I establish the state invariant and failure behavior first, then measure the cost of enforcing it. Rehashing staged blobs during commit is expensive but detects corrupted dependencies before publishing history. LCS is understandable but has explicit scaling limits. I would optimize with a verified fallback and benchmarks, rather than remove checks or claim speed based only on C++ and data-structure choices.

### Q163. What did you intentionally leave out?

Remotes, authentication, packfiles, compression, garbage collection, rename-aware merge, file modes and robust concurrent/crash recovery are outside the current implementation. Each introduces substantial additional state and policy. I would explain which omissions were deliberate in my actual project scope and which became visible during review. A credible answer connects scope to a focused learning objective rather than pretending the missing features were unnecessary in a real VCS.

### Q164. How do you respond when a reviewer finds a bug?

I first reproduce the behavior in an isolated case, identify the violated invariant and add a regression that fails before the fix. Then I make the smallest coherent correction, run relevant checks and state remaining limits. This review followed that pattern for three concrete failures. I would report the outcome without claiming the new tests prove all correctness or using a passing test count to dismiss further review.

### Q165. What would you do with another week?

I would first improve the highest-impact reliability gap: mutable metadata updates and recovery. I would specify a single-writer lock and atomic replacement protocol, add failure injection around index/ref writes, and document expected recovery states. Next I would test the merge-base heuristic against adversarial DAGs and improve it. I would choose a small completed improvement with evidence rather than promise remotes, a new diff engine and full compatibility in one week.

### Q166. What would you do differently in a team?

Write down repository formats, command contracts, invariants and ownership boundaries so changes in storage, CLI and merge logic remain compatible. Require tests around destructive state transitions and review the difference between guaranteed and best-effort behavior. Maintain a roadmap that separates educational experiments from supported features. The current source does not establish that this was a team project, so I would answer this as a hypothetical collaboration approach unless I have real team experience to describe.

### Q167. How did references or AI assistance affect the project?

Give a truthful account of where you used algorithm descriptions, libraries, generated code or review help. SHA-256 is a standardized algorithm and Catch2 is vendored third-party testing code; neither should be presented as your invention. Demonstrate ownership by explaining invariants, identifying limitations and making a small change yourself. Understanding and transparency are stronger than an unverifiable claim that every line was written without assistance.

### Q168. What is the strongest evidence that you understand the project?

I can trace add through hashing and index persistence, explain why commit saves staged bytes rather than current bytes, predict merge outcomes from a three-snapshot table, and discuss a failing edge case with its regression. I can also distinguish implemented behavior from proposed improvements. An interviewer can change one file or graph shape and ask me to predict the result. That is more persuasive than memorizing a feature list.

# 17 | Live demo and troubleshooting playbook

## Before the interview

Build and test first. Use a fresh demo directory outside E:\GROOT to avoid its legacy metadata. Set the matching MinGW runtime PATH and invoke the executable by absolute path. The script uses explicit UTF-8 encoding because shell echo behavior can vary.

## Reproducible build in this environment

```powershell
$cmake = 'E:\CLion 2026.2.1\bin\cmake\win\x64\bin\cmake.exe'
$ctest = 'E:\CLion 2026.2.1\bin\cmake\win\x64\bin\ctest.exe'
$env:PATH = 'E:\CLion 2026.2.1\bin\mingw\bin;' + $env:PATH
& $cmake -S E:\GROOT -B E:\GROOT\build\interview-verify -G Ninja `
  '-DCMAKE_MAKE_PROGRAM=E:/CLion 2026.2.1/bin/ninja/win/x64/ninja.exe' `
  '-DCMAKE_CXX_COMPILER=E:/CLion 2026.2.1/bin/mingw/bin/g++.exe' `
  -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'
& $cmake --build E:\GROOT\build\interview-verify
& $ctest --test-dir E:\GROOT\build\interview-verify --output-on-failure
& E:\GROOT\build\interview-verify\groot_tests.exe
python E:\GROOT\tools\interview_smoke.py E:\GROOT\build\interview-verify\groot.exe
```

On another machine, replace the toolchain paths and use its supported CMake generator.

## Seven-minute demo: show the model, not just commands

```powershell
$groot = 'E:\GROOT\build\interview-verify\groot.exe'
$demo = Join-Path $env:TEMP ('groot-interview-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $demo | Out-Null
Set-Location -LiteralPath $demo
$utf8 = [System.Text.UTF8Encoding]::new($false)
function Put-Text($name, $text) {
  [System.IO.File]::WriteAllText((Join-Path $demo $name), $text, $utf8)
}
& $groot init
Put-Text 'story.txt' "one`ntwo`nthree`n"
& $groot status
& $groot add story.txt
& $groot diff --staged
& $groot commit -m 'base'
& $groot branch feature
& $groot checkout feature
Put-Text 'story.txt' "ONE`ntwo`nthree`n"
& $groot add story.txt
& $groot commit -m 'feature edits first line'
& $groot checkout main
Put-Text 'story.txt' "one`ntwo`nTHREE`n"
& $groot add story.txt
& $groot commit -m 'main edits last line'
& $groot merge feature
Get-Content story.txt
& $groot log
& $groot status
Get-Content .groot\HEAD
Get-Content .groot\index
```

Checkpoints: untracked -> staged -> committed; branch checkout restores snapshots; merge combines ONE and THREE. HEAD still names main. Inspect the newest commit record for both parents; log displays first-parent history.

## Three-minute conflict extension

```powershell
& $groot branch conflict
& $groot checkout conflict
Put-Text 'story.txt' "LEFT`ntwo`nTHREE`n"
& $groot add story.txt
& $groot commit -m 'left'
& $groot checkout main
Put-Text 'story.txt' "RIGHT`ntwo`nTHREE`n"
& $groot add story.txt
& $groot commit -m 'right'
& $groot merge conflict
Get-Content story.txt
& $groot status
& $groot commit -m 'must fail while unstaged'
Put-Text 'story.txt' "RESOLVED`ntwo`nTHREE`n"
& $groot add story.txt
& $groot commit -m 'resolve deliberately'
& $groot status
```

To demonstrate abort instead, run merge --abort immediately after displaying markers, before the resolution commands. Explain that abort restores original tracked content and may discard resolution edits. Keep the demo directory for inspection; do not run destructive cleanup against a guessed path during the interview.

## Common failures and what to say

- Missing cmake or runtime DLL: use the absolute tool path and matching MinGW bin on PATH.
- Repository/init/old-format error: confirm cwd and use a new existing demo directory outside the source checkout.
- Checkout refused: inspect tracked edits and untracked path collisions; do not remove files blindly.
- LCS too large: explain the 10-million-cell guard and use smaller inputs.
- Conflict returns code 0: inspect the reported merge state rather than assuming completion from the exit code.

### Q169. What will you demonstrate first if given only two minutes?

I would initialize a fresh repository, add and commit a file, then stage one version and edit it again before committing. I would show that the commit contains the staged version and status still reports the later edit. This small demo proves the index/working-tree distinction more effectively than rushing through nine command names. If time remains, I would inspect the blob and index to connect behavior to storage.

### Q170. What if the interviewer changes the demo unexpectedly?

I would predict the state transition before running the command: which snapshot is H, what is in I, and what bytes are in W. For branch/merge questions I would draw parent links and identify the base relationship. If the outcome differs, I would inspect metadata and report a possible bug or limitation rather than improvise a guarantee. A reliable mental model is more useful than a memorized transcript.

# 18 | Whiteboard exercises with worked answers

## Exercise A: stage, edit, commit

Start with H: a.txt -> hash(A). Edit to B, add, edit to C, commit. Answer: I is B after add and remains B; the new commit records B; W stays C; status reports a.txt as modified but not staged relative to the new commit. The old A blob remains, and B is now reachable. C need not exist as a stored blob because it was not added.

## Exercise B: exact deduplication

Add x.txt = 'hello' and y.txt = 'hello'. Answer: two index paths refer to one object digest. Rename y.txt to z.txt by deletion/add and commit. There is still one blob for 'hello', but the manifest path changes. No rename event is stored. If x.txt changes to 'Hello', the new bytes require a separate object; the other path still refers to the original digest.

## Exercise C: path-level merge

Base: a=A, b=B. Ours: a=A2, b=B. Theirs: a=A, b absent, c=C. Answer: a takes ours because theirs equals base; b is deleted because ours equals base; c takes theirs because ours and base are both absent. A divergent history creates a two-parent commit with a=A2 and c=C, assuming no path collisions or working-state blockers.

## Exercise D: same-file text merge

Base lines [one,two,three]. Ours [ONE,two,three]. Theirs [one,two,THREE]. Answer: the first replacement occupies base range [0,1), the second [2,3); they are disjoint, so output [ONE,two,THREE]. If both replace [0,1) with different lines, conflict. If both replace it with the same line, deduplicate. Preserve trailing-newline distinctions when translating the lists back to bytes.

## Exercise E: why nearest common ancestor can be wrong

Construct A -> B -> C1 -> C2 -> C3 -> C4 -> C5 in development order. Create E from A on another branch, then M with parents E and C5. B is an ancestor of M via the long chain, but A has a much shorter path from M via E. If merging M into B, the correct result is a fast-forward. A minimum-distance rule can prefer A. The reviewed fix first tests whether B is reachable from M, so the ancestry case is handled correctly.

## Exercise F: crash ordering

Suppose a commit record is durable but the branch ref update has not happened. Answer: previous branch history is still reachable, and the new record can be orphaned. Suppose the direct ref write was truncated. Answer: the ref may be invalid, and no built-in reflog/recovery mechanism restores it. Propose a lock, temporary ref, expected-old-tip check, durable replacement and a recovery log; explain that each step addresses a different failure mode.

## Exercise G: GC roots

Given two branch tips, a staged blob not in any commit and an active MERGE_HEAD, which objects are live? Answer: traverse both branch histories and both parent edges; mark all referenced blobs; also root the index's staged blobs and merge state. An object created by a failed add with no index/ref reference is a collection candidate only under a safe writer-coordination and grace policy. Never sweep based solely on the active branch.

## Exercise H: LCS matrix sizing

For 3,000 lines on each side, (3001*3001) = 9,006,001 cells fits under the 10-million-cell guard. At eight bytes per size_t that is about 72 MB for the matrix alone. For 4,000 lines on each side, 16,008,001 cells exceeds the guard. Line text, vectors and output add memory beyond the matrix. This makes 'a few thousand lines' a concrete scale limit, not an abstract big-O footnote.

# 19 | Mock interviews and study schedule

## Mock interview 1: general software engineer, 25 minutes

- 0-2 minutes: introduce Groot using chapter 3. A strong answer covers the problem, three-state model and one concrete limitation.
- 2-7 minutes: explain add and commit, then solve exercise A. Use Q019, Q028 and Q029 as the answer key.
- 7-12 minutes: explain hashing, deduplication and corruption boundaries. Use Q035-Q046 and exercise B.
- 12-17 minutes: draw a divergent graph and explain merge. Use Q069-Q082 and exercises C/D.
- 17-22 minutes: discuss one regression and a reliability improvement. Use Q123, Q164 and exercise F.
- 22-25 minutes: answer the CV performance/data-structure challenges using Q007-Q011. State evidence and avoid absolute claims.

## Mock interview 2: systems/C++ interviewer, 35 minutes

- 0-5 minutes: architecture, RAII, string_view lifetime and exception boundaries. Answer key: Q024, Q088-Q096.
- 5-12 minutes: immutable publication versus durable metadata updates. Answer key: Q031-Q034, Q045 and Q114-Q117.
- 12-20 minutes: graph reachability, shortcut ancestry and multiple bases. Answer key: Q070-Q073 and exercise E.
- 20-27 minutes: LCS recurrence, memory cap, interval merging and repeated lines. Answer key: Q059-Q068, Q080 and exercise H.
- 27-32 minutes: source-derived complexity and benchmarking. Answer key: Q101-Q110, Q159.
- 32-35 minutes: define a safe next change and its tests. Answer key: Q128, Q137 and Q165.

## Follow-up ladders

- 'Why SHA-256?' -> stable identity -> deduplication -> collision versus corruption -> authenticity limits -> streaming. Use Q035, Q037, Q039 and Q047.
- 'Why an index?' -> select snapshot -> staged plus unstaged -> deletion semantics -> atomic index update. Use Q019, Q029-Q031 and Q117.
- 'How does merge work?' -> base decision table -> same-file edits -> conflicts -> reachability bug -> general base limitation. Use Q006, Q072-Q082.
- 'Why C++?' -> real features -> ownership -> I/O memory -> measured performance. Use Q014, Q088-Q100 and Q104.
- 'Is it safe?' -> expected collision guards -> partial failure -> concurrent writes -> recovery design. Use Q033-Q034 and Q111-Q118.

## Seven-day preparation plan

- Day 1: rehearse both introductions; rewrite the CV using precise claims; inspect every public header.
- Day 2: run the build/tests/demo; trace add and commit; hand-inspect index, object and commit bytes.
- Day 3: implement the SHA-256 explanation on paper; understand padding boundaries; compare object IDs with hashlib.
- Day 4: hand-compute a small LCS table; explain reconstruction, newlines and empty-file existence.
- Day 5: draw branch DAGs and solve every merge exercise; rehearse conflicts and abort without notes.
- Day 6: explain reliability and performance limits; design one concrete improvement and meaningful tests.
- Day 7: perform both mocks aloud, record yourself, fix vague answers and run the demo once more.

## Night-before checklist

Be able to state the exact feature scope, defend every CV noun, draw H/I/W, trace add/commit/checkout/merge, quote the correct test counts, explain one bug accurately, show the demo and name two limits plus the next improvement. Know which changes came from the current review. Do not memorize every SHA constant or recite every question; understand the transformations and invariants.

## A self-scoring rubric

Score each answer from 0 to 3: 0 = incorrect or invented; 1 = definition only; 2 = correct code-specific explanation; 3 = code-specific explanation plus tradeoff, edge case or evidence. Aim for 3 on the introduction, index, hashing, commits, merge and CV claims. If you cannot explain a topic without reading, revisit its code rather than memorizing a longer paragraph. The goal is reliable reasoning under follow-up.

# 20 | Source catalog, verification commands and glossary

## Source map for code navigation

Paths are relative to E:\GROOT. Function names are the stable navigation anchors; line numbers below refer to the reviewed snapshot and may shift with later edits.

- S01 - CMakeLists.txt: C++20 requirement, groot_core, CLI, Catch2 test target and one CTest registration. Repository source path case corrected in review.
- S02 - include/groot/repository.hpp: Status, Commit, MergeResult and Repository API. Start here to learn the domain model.
- S03 - src/main.cpp: command dispatch, status formatting, argument validation and exit behavior. Text merge conflicts currently return 0.
- S04 - src/Repository.cpp: metadata helpers around lines 28-134; serialize/load/save_commit around 136-215; snapshot validation/application around 238-314.
- S05 - src/Repository.cpp: ancestor_distances around 316; merge_base around 332; discovery/init around 365/386; add around 405; status around 478.
- S06 - src/Repository.cpp: commit around 513; first-parent log around 564; diff around 577; branches around 622; checkout around 644; merge around 657; abort around 734.
- S07 - include/groot/index.hpp and src/index.cpp: ordered mapping, path/hash validation, parse/save format and API.
- S08 - include/groot/object_store.hpp and src/object_store.cpp: digest-named objects, publication and verified reads.
- S09 - include/groot/sha256.hpp and src/sha256.cpp: SHA-256 padding, schedule, rounds and output.
- S10 - include/groot/diff.hpp and src/diff.cpp: line splitting around 14, edit extraction around 35, line_diff around 71, merge_lines around 118.
- S11 - include/groot/utils.hpp and src/utils.cpp: binary file I/O and final stream error checks.
- S12 - tests/foundation_test.cpp: repository, SHA-256 and object integrity tests; Catch2 main is defined here.
- S13 - tests/staging_test.cpp and tests/commit_test.cpp: index, staging, status and history snapshot tests.
- S14 - tests/history_test.cpp: diff, checkout, merge and review regressions beginning around 172.
- S15 - tools/interview_smoke.py: standalone temporary-repository CLI verification and independent hashlib checks.
- S16 - README.md: project overview, build/demo commands and declared limits. Its complexity table is a simplified summary; chapter 11 expands actual implementation costs.
- S17 - third_party/catch.hpp and Catch2-LICENSE.txt: vendored third-party testing framework, not the product's VCS implementation.

The active source, headers, CMake configuration and all four test files were inspected. archive contains an older prototype and is not part of groot_core. The CV text in chapter 2 was supplied by the user in this conversation; no separate CV file was available or required after that text was provided.

## External primary references

These support background concepts and comparisons; current Groot behavior is grounded in local source and tests. Accessed during the review on 11 November 2026. No prose is quoted from these references.

- R01 - NIST, FIPS 180-4, Secure Hash Standard, sections 5 and 6.2: padding and SHA-256 algorithm definition. https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
- R02 - Git, Pro Git, Git Internals: Git Objects: conceptual comparison with typed Git objects and content-addressed storage. https://git-scm.com/book/en/v2/Git-Internals-Git-Objects
- R03 - Git, git-merge-base manual: best-common-ancestor definition and possibility of multiple merge bases. https://git-scm.com/docs/git-merge-base
- R04 - CMake, ctest manual: test-driver behavior and test-directory invocation. The local run used CMake 4.3.1; the online latest manual is background documentation, not proof of the installed version. https://cmake.org/cmake/help/latest/manual/ctest.1.html

## Verification artifacts

output/verification/cli-smoke.json records the separate CLI workflow. output/verification/review_report.json records the final build/test summary and source digests. output/verification/test-results.txt preserves final verification output. These support the reviewed working tree, not just the latest Git commit: significant feature changes were already uncommitted when the review began, and this review intentionally preserved them.

## Compact glossary

- Blob: one exact file byte sequence stored under its digest.
- Content addressing: naming stored data using a function of its bytes.
- Snapshot: complete mapping from tracked file paths to blob identifiers.
- Index: mutable next-snapshot mapping selected by staging.
- Working tree: files currently present and editable on disk.
- Commit: immutable snapshot record plus history metadata.
- Ref: mutable name pointing to a tip commit, or empty for an unborn branch.
- Symbolic HEAD: reference naming the active branch ref.
- DAG: directed acyclic graph, the intended structure of commit history.
- First parent: previous current-branch tip recorded by a commit.
- Merge parent: other branch tip recorded by a divergent merge.
- Fast-forward: moving a branch ref to a descendant already containing its history.
- Merge base: common-ancestor input used to distinguish one-sided and two-sided changes.
- LCS: longest common subsequence; ordered matching elements need not be contiguous.
- Canonical encoding: one prescribed byte representation for a logical record.
- Atomic publication: a narrow publish step that avoids exposing/replacing a destination partway through that step.
- Durability: completed state survives the failures promised by a storage contract.
- Transaction: a coordinated state transition with defined atomicity/recovery behavior.
- TOCTOU: a filesystem or state change between checking a condition and using the checked resource.
- Deduplication: sharing storage for equal content; in Groot this is exact whole-file reuse.
- Reachability: existence of a parent-edge path from one commit to another.
- Garbage collection: reclaiming data unreachable from all protected roots under a safe update policy.

## Closing rehearsal prompt

Explain Groot in two minutes. Trace one file through working tree, index, object store and commit. Draw a divergent merge. Defend each CV claim with one observable behavior. State one remaining correctness limit and one measured fact. If you can do those without notes, use the question bank to deepen weak areas instead of memorizing more adjectives.

