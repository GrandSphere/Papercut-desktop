# Papercut

Papercut is a PDF reader for Windows and Linux. 

## License

Papercut is licensed under the GNU General Public License v3.0 (GPL-3.0).

MuPDF is licensed under the AGPL.

Roboto and Material Symbols fonts in `assets/fonts/` are licensed under Apache 2.0 (see `assets/fonts/*-LICENSE.txt`).

Copyright © 2025 Grand Sphere Studios

## Features

- PDF viewing: Which is probably obvious.
- Folios: Keep often used documents in a folio.
- Bookmarks: Mark pages.
- Combine and rearrange: Merge reorder pages.
- Share: Choose pages to share.
- CLI: Share-to / share-from via stdin and stdout.
- Themes: Custom document colouring.

## Technical Information

- Language: C++20
- UI: Qt 6 Quick / QML
- PDF engine: MuPDF 1.28.0
- Build: CMake 3.22+, Ninja
- Platforms: Windows (MinGW + Qt), Linux (GCC/Clang + Qt)
- License: GPL-3.0 (application); AGPL (MuPDF when distributed)

## Getting Started

### Prerequisites

Prerequisites listed in `requirements.txt`:

- CMake 3.22 or newer
- MuPDF 1.28.0 (fetched automatically unless `USE_SYSTEM_MUPDF=ON`)
- Ninja
- Qt 6.8 or newer with modules: Core, Gui, Sql, Network, Quick, QuickControls2, Qml
- SQLite (via Qt Sql)
- C++20 compiler

### Build

1. Clone the repository:

   ```bash
   git clone https://github.com/GrandSphere/Papercut-desktop.git
   cd Papercut-desktop
   ```
2. Configure and build (from the repository root):

   ```bash
   cmake -S . -B Compile -G Ninja -DCMAKE_BUILD_TYPE=Release -DPAPERCUT_BUILD_GUI=ON
   cmake --build Compile
   ```

3. Run the packaged app:

   - Windows: `Release/Windows/bin/`
   - Linux: `Release/Linux/bin/`

4. Optional tests (after build):

   ```bash
   ctest --test-dir Compile --output-on-failure
   ```
## Known Issues

Report issues at https://github.com/GrandSphere/Papercut-desktop/issues.

## Contact

For questions or feedback, contact:

- vaneeden.a@proton.me
- esterhuyse.a@proton.me
