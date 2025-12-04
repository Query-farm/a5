# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a DuckDB extension that provides A5 geospatial indexing functions. A5 is a pentagonal geospatial index system with millimeter-accurate, equal-area cells across 31 resolution levels.

**Architecture**: The extension uses a hybrid C++/Rust implementation:
- **Rust core** (`a5_rust/`): Provides FFI bindings to the upstream `a5` crate (v0.6.2)
- **C++ wrapper** (`src/`): Implements DuckDB scalar functions that call into Rust via C FFI
- **Build system**: CMake with Corrosion for Rust integration

## Build Commands

### Development Builds
```bash
# Debug build (for development)
make debug

# Release build (optimized)
make release
```

### Testing
```bash
# Run all tests (uses release build)
make test

# Run tests with debug build
make test_debug
```

Tests are written as SQLLogicTests in `test/sql/a5.test`.

### Code Formatting
```bash
# Check code formatting (C++ via clang-format)
make format-check

# Auto-fix formatting issues
make format-fix
```

### Cleaning
```bash
# Clean build artifacts
make clean
```

## Architecture Details

### FFI Layer (Rust ↔ C++)

The Rust FFI bindings are in `a5_rust/src/lib.rs`. Key patterns:

1. **Error handling**: Rust functions return C-compatible structs (`ResultU64`, `ResultLonLat`) with nullable error pointers
2. **Memory management**: Array returns (`LonLatDegreesArray`, `CellArray`) transfer ownership to C++; must be freed with `a5_free_*` functions
3. **Auto-generated header**: `src/include/rust.h` is generated via cbindgen (configured in `a5_rust/cbindgen.toml`)

### C++ DuckDB Integration

`src/a5_extension.cpp` implements scalar functions using DuckDB's executor framework:
- `UnaryExecutor`: Single input parameter (e.g., `a5_cell_area`, `a5_get_resolution`)
- `BinaryExecutor`: Two input parameters (e.g., `a5_cell_to_parent`)
- `TernaryExecutor`: Three input parameters (e.g., `a5_lonlat_to_cell`)
- Manual loops for array outputs (e.g., `a5_cell_to_boundary`, `a5_cell_to_children`)

All functions validate that resolution parameters are ≤30 (MAX_RESOLUTION constant).

### Build System

CMake configuration (`CMakeLists.txt`) handles:
- Platform-specific Rust target detection (macOS: aarch64/x86_64-apple-darwin, Linux: gnu/musl, Windows: gnu/msvc)
- Corrosion integration (v0.5.2) for Rust compilation
- Links `a5_rust-static` (static extension) or `a5_rust` (loadable extension)

The extension uses DuckDB's extension-ci-tools (git submodule) for standardized build infrastructure.

## Development Workflows

### Adding New A5 Functions

1. Add Rust FFI binding in `a5_rust/src/lib.rs`:
   - Use `#[no_mangle] pub extern "C"` for function signature
   - Return C-compatible result types
   - Handle Result<T, E> from upstream `a5` crate

2. Regenerate C header: `cbindgen` runs automatically during build, updating `src/include/rust.h`

3. Implement DuckDB function in `src/a5_extension.cpp`:
   - Choose appropriate executor (Unary/Binary/Ternary)
   - Add input validation
   - Register in `A5Extension::Load()` via `CreateScalarFunctionInfo`

4. Add tests in `test/sql/a5.test` (SQLLogicTest format)

### Working with Rust Dependencies

- Update `a5` crate version in `a5_rust/Cargo.toml`
- Run `cargo update` in `a5_rust/` directory
- Rebuild with `make clean && make debug`

### Platform-Specific Builds

The build system auto-detects Rust targets. For cross-compilation, set `OSX_BUILD_ARCH` (macOS) or use appropriate `DUCKDB_PLATFORM` values:
```bash
# macOS ARM64
make release OSX_BUILD_ARCH=arm64

# macOS x86_64
make release OSX_BUILD_ARCH=x86_64
```

## Git Submodules

This repo has two submodules:
- `duckdb/` - DuckDB source code (main branch)
- `extension-ci-tools/` - Build system utilities

Update submodules: `git submodule update --init --recursive`

## Extension Distribution

The extension is published as a DuckDB Community Extension. Users install via:
```sql
INSTALL a5 FROM community;
LOAD a5;
```

CI/CD is configured in `.github/workflows/MainDistributionPipeline.yml`.
