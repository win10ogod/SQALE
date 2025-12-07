# SQALE Installation Guide

This guide covers building and installing SQALE on various platforms.

## Quick Start

```bash
# Clone the repository
git clone https://github.com/yourusername/SQALE.git
cd SQALE

# Build
make

# Run the REPL
./build/sqale repl

# Run a program
./build/sqale run examples/hello.sq
```

## Requirements

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential
```

### Linux (Fedora/RHEL)

```bash
sudo dnf install gcc make
```

### macOS

Install Xcode Command Line Tools:
```bash
xcode-select --install
```

Or install via Homebrew:
```bash
brew install gcc make
```

### Windows

**Option 1: MSYS2 (Recommended)**

1. Download and install [MSYS2](https://www.msys2.org/)
2. Open MSYS2 MINGW64 terminal
3. Install dependencies:
   ```bash
   pacman -S mingw-w64-x86_64-gcc make
   ```
4. Build:
   ```bash
   make
   ```

**Option 2: WSL (Windows Subsystem for Linux)**

1. Install WSL: `wsl --install`
2. Open Ubuntu and follow Linux instructions above

## Building

### Standard Build

```bash
make
```

This creates `./build/sqale`.

### Build Options

| Command | Description |
|---------|-------------|
| `make` | Standard optimized build |
| `make clean` | Remove all build artifacts |
| `make run` | Build and run `examples/hello.sq` |
| `make repl` | Build and start the REPL |

### Debug Build

```bash
make CFLAGS="-std=c11 -O0 -g -Wall -Wextra -Werror -Iinclude -DUSE_LLVM=0"
```

### Build with Sanitizers (for development)

```bash
make CFLAGS="-std=c11 -O0 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -DUSE_LLVM=0"
```

### LLVM Backend (Optional)

To enable the LLVM backend for native code generation:

```bash
# Install LLVM (Ubuntu)
sudo apt-get install llvm-dev

# Build with LLVM
make USE_LLVM=1
```

## Verification

After building, run the test suite:

```bash
# Run smoke tests
bash scripts/run_tests.sh

# Run individual examples
./build/sqale run examples/hello.sq
./build/sqale run examples/functions.sq
./build/sqale run examples/test_operators.sq
./build/sqale run examples/test_new_features.sq
```

## Installation (System-wide)

### Linux/macOS

```bash
# Install to /usr/local/bin (requires sudo)
sudo cp build/sqale /usr/local/bin/

# Or install to user directory
mkdir -p ~/.local/bin
cp build/sqale ~/.local/bin/
# Add to PATH: export PATH="$HOME/.local/bin:$PATH"
```

### Windows

Copy `build/sqale.exe` to a directory in your `PATH`, or add the `build/` directory to your `PATH` environment variable.

## REPL Usage

Start the interactive REPL:

```bash
./build/sqale repl
```

Example session:
```lisp
SQALE REPL. Ctrl-D to exit.
> [+ 1 2]
3
> [def double : [Int -> Int] [fn [[x : Int]] : Int [* x 2]]]
> [double 21]
42
> [let [[x : Int 10]] [* x x]]
100
```

## Running Programs

```bash
# Run a .sq file
./build/sqale run path/to/program.sq

# Example
./build/sqale run examples/hello.sq
```

## File Extension

SQALE source files use the `.sq` extension.

## Environment Variables

| Variable | Description |
|----------|-------------|
| `SQALE_PATH` | Additional search paths for imports (colon-separated) |

## Troubleshooting

### "command not found: make"

Install build tools for your platform (see Requirements above).

### Linker errors about math functions

Ensure `-lm` is linked. This should be automatic with the Makefile.

### Permission denied

```bash
chmod +x build/sqale
```

### Windows: "not recognized as internal or external command"

Use MSYS2 terminal or WSL, not Command Prompt.

## Next Steps

- Read `CLAUDE.md` for project documentation
- Explore `examples/` for sample programs
- Check `docs/LANGUAGE_COMPLETENESS.md` for language features

## Getting Help

- GitHub Issues: [Report bugs or request features](https://github.com/yourusername/SQALE/issues)
- See `CLAUDE.md` for detailed development documentation
