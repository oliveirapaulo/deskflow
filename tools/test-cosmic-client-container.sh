#!/usr/bin/env bash
# Run the locally compiled client against the real COSMIC session without
# replacing the installed Flatpak or changing its settings.
set -euo pipefail

if [[ "${1:-}" == "--help" ]]; then
  printf '%s\n' 'Usage: bash tools/test-cosmic-client-container.sh [--native-hid] [--gui]' \
    'Default: run the patched Deskflow client with wl-clipboard in an isolated Fedora test container.' \
    '--native-hid: run the native-HID build, expose the Logitech receiver, and switch MX Keys to slot 2 on leaving Pop.' \
    '--gui: run our compiled Deskflow GUI against the same isolated settings, including its TLS trust prompt.' \
    'Stop the existing Deskflow client first; press Ctrl+C to end the test.'
  exit 0
fi

native_hid=false
gui_mode=false
for option in "$@"; do
  case "$option" in
    --native-hid) native_hid=true ;;
    --gui) gui_mode=true ;;
    *) printf 'Unexpected argument: %s\n' "$option" >&2; exit 2 ;;
  esac
done

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
app_id=org.deskflow.deskflow
source_config="$HOME/.var/app/$app_id/config/Deskflow"
build_dir=build-fedora43
test_home="$HOME/.local/state/deskflow-cosmic-handoff/container-home"
native_docker_args=()
if "$native_hid"; then
  build_dir="${DESKFLOW_TEST_BUILD_DIR:-build-native-hid}"
  test_home="$HOME/.local/state/deskflow-cosmic-handoff/container-home-native"

  # Match the receiver by its kernel HID identity, not by a fragile hidraw number.
  receiver_paths=()
  for receiver_uevent in /sys/class/hidraw/hidraw*/device/uevent; do
    if [[ -f "$receiver_uevent" ]] && rg -q '^HID_ID=0003:0000046D:0000C52B$' "$receiver_uevent"; then
      receiver_paths+=("/dev/$(basename "$(dirname "$(dirname "$receiver_uevent")")")")
    fi
  done
  if (( ${#receiver_paths[@]} != 1 )); then
    printf 'Expected one Logitech 046d:c52b receiver; found %d.\n' "${#receiver_paths[@]}" >&2
    exit 1
  fi
  # Docker recreates --device nodes without the host's logind ACL. Give the
  # unprivileged client the node's group instead of running Deskflow as root.
  native_docker_args=(--device "${receiver_paths[0]}:${receiver_paths[0]}" \
    --group-add "$(stat -c %g "${receiver_paths[0]}")" \
    --mount 'type=bind,src=/run/udev,dst=/run/udev,readonly')
fi
if [[ ! "$build_dir" =~ ^build-[a-zA-Z0-9_-]+$ ]]; then
  printf 'Invalid build directory: %s\n' "$build_dir" >&2
  exit 2
fi
test_config="$test_home/.var/app/$app_id/config/Deskflow"
runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
wayland_display="${WAYLAND_DISPLAY:-wayland-0}"
container_name="deskflow-cosmic-test-$(id -u)"

if [[ "${XDG_SESSION_TYPE:-}" != wayland ]]; then
  printf 'This test requires the current COSMIC Wayland session.\n' >&2
  exit 1
fi
if [[ ! -S "$runtime_dir/bus" || ! -S "$runtime_dir/$wayland_display" ]]; then
  printf 'Missing session bus or Wayland socket under %s.\n' "$runtime_dir" >&2
  exit 1
fi
if [[ ! -x "$repo_dir/$build_dir/bin/deskflow-core" ]]; then
  printf 'Missing compiled deskflow-core under %s/%s.\n' "$repo_dir" "$build_dir" >&2
  exit 1
fi
if "$gui_mode" && [[ ! -x "$repo_dir/$build_dir/bin/deskflow" ]]; then
  printf 'Missing compiled Deskflow GUI under %s/%s.\n' "$repo_dir" "$build_dir" >&2
  exit 1
fi
if flatpak ps --columns=application | rg -q "^${app_id//./\\.}$"; then
  printf 'Deskflow Flatpak is still running. Stop it before this test.\n' >&2
  exit 1
fi
if docker ps --format '{{.Names}}' | rg -q "^${container_name}$"; then
  printf 'A Deskflow test container is already running.\n' >&2
  exit 1
fi

install -d -m 0700 "$test_home" "$test_config"
if [[ ! -f "$test_config/Deskflow.conf" ]]; then
  if [[ -f "$source_config/Deskflow.conf" ]]; then
    cp -a "$source_config/." "$test_config/"
    if [[ -f "$test_config/tls/deskflow.pem" ]]; then
      chmod 0600 "$test_config/tls/deskflow.pem"
    fi
  else
    # GUI-first bootstrap works without any installed Deskflow Flatpak.
    printf '[client]\n' > "$test_config/Deskflow.conf"
  fi
fi
if ! "$gui_mode" && ! rg -q '^remoteHost=' "$test_config/Deskflow.conf"; then
  printf 'No server configured. Run this launcher with --gui first.\n' >&2
  exit 1
fi
if "$native_hid"; then
  # This is an isolated copy; the Flatpak settings are never edited.
  # The custom GUI and core share this trust file; do not overwrite a newly
  # accepted server fingerprint with the older Flatpak trust database.
  if rg -q '^mxKeysHostOnScreenLeave=' "$test_config/Deskflow.conf"; then
    sed -i 's/^mxKeysHostOnScreenLeave=.*/mxKeysHostOnScreenLeave=2/' "$test_config/Deskflow.conf"
  else
    sed -i '/^\[client\]/a mxKeysHostOnScreenLeave=2' "$test_config/Deskflow.conf"
  fi
fi

log_dir="$HOME/.local/state/deskflow-cosmic-handoff"
log_file="$log_dir/client-$(date +%Y%m%d-%H%M%S).log"
printf 'Starting patched client. Log: %s\n' "$log_file"
if "$gui_mode"; then
  printf 'Using our compiled GUI with isolated settings. Accept the new server fingerprint here.\n'
else
  printf 'Cross into Pop; test focus and plain-text clipboard in both directions. Press Ctrl+C to stop.\n'
fi
if "$native_hid"; then
  printf 'Native HID enabled: on leaving Pop, MX Keys switches to Windows slot 2; MX Master is untouched.\n'
fi

if "$gui_mode"; then
  client_command=("/work/$build_dir/bin/deskflow")
else
  client_command=("/work/$build_dir/bin/deskflow-core" client \
    -s "/home/paulo/.var/app/$app_id/config/Deskflow/Deskflow.conf")
fi

docker run --rm --init --name "$container_name" \
  --user "$(id -u):$(id -g)" --network host \
  "${native_docker_args[@]}" \
  --mount "type=bind,src=$repo_dir,dst=/work,readonly" \
  --mount "type=bind,src=$test_home,dst=/home/paulo" \
  --mount "type=bind,src=$runtime_dir,dst=$runtime_dir" \
  --env HOME=/home/paulo \
  --env XDG_RUNTIME_DIR="$runtime_dir" \
  --env WAYLAND_DISPLAY="$wayland_display" \
  --env DBUS_SESSION_BUS_ADDRESS="unix:path=$runtime_dir/bus" \
  --env XDG_SESSION_TYPE=wayland \
  --env XDG_CURRENT_DESKTOP=COSMIC \
  --env QT_QPA_PLATFORM=wayland \
  --env XDG_CONFIG_HOME="/home/paulo/.var/app/$app_id/config" \
  --env XDG_STATE_HOME="/home/paulo/.var/app/$app_id/.local/state" \
  --workdir /work \
  deskflow-test-fedora43:local \
  "${client_command[@]}" \
  2>&1 | tee "$log_file"
