# 🧚 GNL-fairy

A comprehensive tester for 42's **get_next_line** project, featuring memory leak detection, multi-buffer-size validation, forbidden-symbol checks, and rigorous line-by-line comparison against a reference splitter.

## ✨ Features

- ✅ Complete functional tests (mandatory + bonus multi-fd)
- 🧹 Clean execution (temporary files created and removed automatically)
- 🔍 Automatic verification with **norminette**
- 📋 Prototype validation in project headers
- 🎛️ Recompiles and replays the suite for several `BUFFER_SIZE` values (1, 42, 9999, 10000000)
- 🔎 External function check (`read`, `malloc`, `free` only — catches `lseek`)
- 🌍 Global-variable detection (forbidden by the subject)
- 💧 Memory leak detection with **Valgrind**, including `read` error and `malloc` failure paths
- ⏱️ Per-test timeout guard against infinite loops
- 📊 Clear and colorful output
- 🔇 Optional verbose mode for detailed results

## 📋 Prerequisites

Before using GNL-fairy, make sure you have installed:

- `gcc` (with `--wrap` linker support)
- `valgrind`
- `norminette`
- `bash`
- `nm` (usually pre-installed)

## 🚀 Installation

1. Clone the repository inside your get_next_line directory:

```bash
git clone https://github.com/sys-harmony/gnl-fairy.git
```

2. Navigate to the tester folder:

```bash
cd gnl-fairy
```

3. Make the script executable:

```bash
chmod +x run.sh
```

## 🎯 Usage

### Run all tests

```bash
./run.sh
```

### Verbose mode (displays all tests, even passing ones)

```bash
./run.sh --verbose
# or
./run.sh -v
```

## 📊 Output

```
╔════════════════════════════════════╗
║            GNL-FAIRY 🧚            ║
╚════════════════════════════════════╝

📝 Checking norm...               Done
🔖 Checking version...           Bonus
📋 Checking prototype...          Done
📂 Checking sources...            Done
🎛️  Checking default BUFFER_SIZE...  Done
🔍 Checking externals...          Done
🌍 Checking globals...            Done
🔨 Building tests...              Done
🧪 Running tests...               Done

╔════════════════════════════════════╗
║         OH MY, YOU PASSED!         ║
╚════════════════════════════════════╝
```

On failure, the failing scenarios are printed per buffer size, along with Valgrind logs when a leak is detected.

## 🧪 Tests Covered

### Pre-compilation Checks

- **Norminette**: validates every `.c` and `.h` file (the tester folder itself is pruned)
- **Prototype**: verifies `char *get_next_line(int fd)` in any project header (`*.h`), tolerating spacing, `char*` vs `char *`, and any parameter name
- **Sources**: ensures `get_next_line.c` and `get_next_line_utils.c` are present
- **Default BUFFER_SIZE**: compiles the sources **without** `-D BUFFER_SIZE` with `-Wall -Wextra -Werror` to confirm a usable default exists
- **External functions**: each object is inspected with `nm -u`; only `read`, `malloc`, `free` are allowed (so `lseek` and any libft symbol are reported)
- **Global variables**: objects are scanned with `nm` for uppercase data symbols (`B/C/D/G/S`); a non-static global is forbidden, while the required `static` variable (lowercase) is correctly ignored

### Functional Scenarios (mandatory)

Each scenario writes a known buffer to a temp file and reads it back line by line, comparing every line to the reference split (each line keeps its trailing `\n`, except a final line with none), then asserts that `get_next_line` returns `NULL` twice at EOF.

#### Tiny inputs
- Empty file, single char with/without newline, lone newline

#### Basic multi-line
- Three lines with/without trailing newline
- Only empty lines, mixed empty and filled lines
- Spaces and tabs preserved

#### Long lines
- 5000-char line with/without newline
- 20000-char giant line

#### Many lines
- 1000 identical short lines, increasing-length lines

#### Fd handling
- Negative fd, closed fd, empty source (`/dev/null`) all return `NULL`
- Reading from standard input (fd `0`)

### Bonus Scenarios (multi-fd)

- Interleaved reads across three fds keep each fd's reading state
- Round-robin full drain of three fds
- Two fds on identical content stay independent

### Memory Leak Tests

Run under **Valgrind** (`--leak-check=full --errors-for-leak-kinds=all`). Every helper reads its file to EOF so a correct implementation frees its static stash (a partial read would legitimately leave a *still reachable* buffer and is not a leak).

- Full read with / without trailing newline
- Long single line (8 KB)
- Many lines
- `read` error path (forced via the read wrapper): must return `NULL` without leaking
- `malloc` failure path (forced via the malloc wrapper): must clean up on every failing allocation

Bonus leak tests cover several descriptors read concurrently, including a `malloc` failure during interleaved reads.

## 📁 Project Structure

```
gnl-fairy/
├── run.sh                    # Main test orchestrator
├── basic_tests.c             # Mandatory functional tests
├── basic_tests_bonus.c       # Bonus multi-fd tests
├── leak_tests.c              # Leak tests (mandatory)
├── leak_tests_bonus.c        # Leak tests (bonus)
├── utils.c                   # Helpers + malloc/read wrappers + scenario engine
├── gnl_fairy.h               # Header file
└── README.md                 # This file
```

## 🔍 Advanced Features

### Buffer-size sweep

`BUFFER_SIZE` is a compile-time constant, so the tester recompiles the whole suite for `1`, `42`, `9999` and `10000000` and replays it each time. The expected output is independent of the buffer size, which is exactly the property the subject asks you to reason about — a single `BUFFER_SIZE`-specific bug (off-by-one around the buffer boundary) is caught on at least one size while the others may stay green.

### Reference splitter

Instead of hard-coding expected lines, the tester derives them from the file content with a small reference splitter (`gnl_expected_next`). The same engine drives the multi-fd bonus tests, where each descriptor keeps its own cursor into its own expected stream.

### Forked tests with timeout

Crash-prone scenarios run in separate processes, so a segfault or double free in one case never takes the tester down. Each child arms an `alarm()` so a non-terminating `get_next_line` (a classic `BUFFER_SIZE` mistake) fails with a timeout instead of hanging the run.

### Fault injection

`__wrap_malloc` and `__wrap_read` (via `-Wl,--wrap=malloc,--wrap=read`) let the leak tests force a `malloc` failure at a chosen call and force `read` to return `-1`, validating that `get_next_line` returns `NULL` and frees everything on the error paths.

### Reading to EOF in leak tests

Leak tests always drain to EOF so the static stash is freed by a correct implementation; this avoids false positives on the legitimate *still reachable* buffer a partial read would leave behind.

## 🐛 Debugging

If a test fails:

1. **Verbose mode**: run `./run.sh -v` to see every scenario, per buffer size
2. **Valgrind logs**: full output is printed when a leak or invalid access is detected
3. **Norminette**: specific errors shown with file and line
4. **Build errors**: captured per target under the run's temporary directory and printed on failure

## ⚠️ Limitations

- Tested only on Linux (uses `fork`, `nm`, Valgrind, and `--wrap` linker support)
- Requires the tester to sit in a subfolder of the project (as cloned)
- The reference splitter assumes text input (binary files are undefined behavior per the subject)
- `BUFFER_SIZE = 0` is not exercised (it cannot read and the subject leaves its behavior to you)

## 🤝 Contributing

Contributions are welcome! Feel free to:

- Open an issue to report a bug
- Suggest new edge cases
- Improve test coverage
- Enhance documentation

## 📜 License

This project is free to use for educational purposes.

## 💖 Credits

Created with ✨ by **sys-harmony**.

---

*If GNL-fairy helped you validate your get_next_line, don't forget to leave a ⭐ on the repo!*
