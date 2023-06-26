# assume rubberband source code placed at the same parent folder
cd ../rubberband
mkdir build

# NOTE:
#  - for my m1 mac, ladspa default plugin does not have arm64 binary
#  - all dependencies or packages were installed via homebrew
#  - >pkg-config --list-all makes sure meson checking from
#  - add homebrew include or lib path for meson default using usr paths
meson setup build --wipe --cross-file cross/macos-arm64.txt -Dextra_include_dirs=/opt/homebrew/include

# build
ninja -C build

