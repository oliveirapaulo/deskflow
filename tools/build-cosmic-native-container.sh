#!/usr/bin/env bash
# Build the Pop/COSMIC GUI and core without replacing the installed Deskflow.
set -euo pipefail

if [[ $# -gt 1 || "${1:-}" == "--help" ]]; then
  printf '%s\n' 'Usage: bash tools/build-cosmic-native-container.sh [build-directory]' \
    'The default build directory is build-native-hid.' \
    'Choose a different directory when the current build is running.'
  exit 0
fi

build_dir="${1:-build-native-hid}"
if [[ ! "$build_dir" =~ ^build-[a-zA-Z0-9_-]+$ ]]; then
  printf 'Invalid build directory: %s\n' "$build_dir" >&2
  exit 2
fi

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
image_name=deskflow-test-fedora43:local

docker build -f "$repo_dir/tools/Dockerfile.cosmic-native" -t "$image_name" "$repo_dir/tools"

docker_args=(
  --rm
  --user "$(id -u):$(id -g)"
  --mount "type=bind,src=$repo_dir,dst=/work"
  --env HOME=/tmp
  --workdir /work
)

docker run "${docker_args[@]}" "$image_name" \
  cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF \
  -DBUILD_INSTALLER=OFF \
  -DBUILD_X11_SUPPORT=OFF \
  -DDESKFLOW_NATIVE_MX_KEYS_HANDOFF=ON

docker run "${docker_args[@]}" "$image_name" \
  cmake --build "$build_dir" -j 4 --target deskflow deskflow-core

printf 'Built GUI and core in %s/%s/bin\n' "$repo_dir" "$build_dir"
