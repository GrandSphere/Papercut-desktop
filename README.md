# Papercut

Papercut is a PDF reader for Windows and Linux. 

## License

Papercut is licensed under the GNU General Public License v3.0 (GPL-3.0).

MuPDF is licensed under the AGPL.

Roboto and Material Symbols fonts in `assets/fonts/` are licensed under Apache 2.0 (see `assets/fonts/*-LICENSE.txt`).

Copyright © 2025 Grand Sphere Studios

## Features

- **PDF viewing**: Which is probably obvious.
- **Folios**: Keep often used documents in a folio.
- **Bookmarks**: Mark pages.
- **Combine and rearrange**: Merge reorder pages.
- **Share**: Choose pages to share.
- **CLI**: Share-to / share-from via stdin and stdout.
- **Themes**: Custom document colouring.

## Technical Information

- **Language**: C++20
- **UI**: Qt 6 Quick / QML
- **PDF engine**: MuPDF 1.28.0
- **Build**: CMake 3.22+, Ninja
- **Platforms**: Windows (MinGW + Qt), Linux (GCC/Clang + Qt)
- **License**: GPL-3.0 (application); AGPL (MuPDF when distributed)

## Getting Started

### Prerequisites

Prerequisites listed in **`requirements.txt`**:

- CMake 3.22 or newer
- MuPDF 1.28.0 (fetched automatically unless `USE_SYSTEM_MUPDF=ON`)
- Ninja
- Qt **6.8** or newer with modules: Core, Gui, Sql, Network, Quick, QuickControls2, Qml
- SQLite (via Qt Sql)
- A **C++20** toolchain (MinGW kit that matches your Qt install on Windows; GCC or Clang on Linux)
- On **Windows**: [Git for Windows](https://git-scm.com/download/win) — MuPDF’s build needs `sh`, `bash`, and Unix `mkdir` from `Git\usr\bin` on `PATH`

MuPDF will be downloaded automatically on first configure unless you pass `-DUSE_SYSTEM_MUPDF=ON`.

### Build

1. Clone the repository:

   ```bash
   git clone https://github.com/GrandSphere/papercut-desktop.git
   cd papercut-desktop
   ```
2. Configure and build (from the repository root):

   ```bash
   cmake --preset default
   cmake --build Compile
   ```

   Without presets:

   ```bash
   cmake -S . -B Compile -G Ninja -DCMAKE_BUILD_TYPE=Release -DPAPERCUT_BUILD_GUI=ON
   cmake --build Compile
   ```

   **Windows (Qt not on PATH):** set `CMAKE_PREFIX_PATH` to your Qt MinGW kit, for example:

   ```bash
   cmake -S . -B Compile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
   cmake --build Compile
   ```

3. Run the **packaged** app:

   - **Windows**: `Release/Windows/papercut.exe` (launcher) and `Release/Windows/bin/` (app + Qt DLLs)
   - **Linux**: `Release/Linux/papercut` and `Release/Linux/bin/`

   CMake creates `Compile/` (build tree, MuPDF source, objects) and `Release/<OS>/` (ready-to-run layout: launcher, `bin/`, `README.md`).

4. Optional tests (after build):

   ```bash
   ctest --test-dir Compile --output-on-failure
   ```
## Known Issues

Report issues at https://github.com/GrandSphere/papercut-desktop/issues.

## Contact

For questions or feedback, contact:

- vaneeden.a@proton.me
- esterhuyse.a@proton.me
