# nvidia-linux-xid39

Minimal reproduction of an NVIDIA GPU channel lockup caused by an
out-of-bounds texture upload in Vulkan (via SDL3's GPU API).

A copy whose destination region extends past the edge of the destination
texture faults the copy engine. On an RTX 5080 with driver `610.57.04` the
kernel reports `Xid 39` (`ROBUST_CHANNEL_CE0_ERROR`) and/or `Xid 109`
(`CTX SWITCH TIMEOUT`), and the GPU usually stays wedged: the display server
dies with `Xid 119` and recovery needs a reboot.

The copy is invalid usage on the application's part. The bug is that invalid
usage from an unprivileged process takes down the whole machine instead of
failing the copy or, at worst, the offending channel.

See `bug_crash.md` for the full analysis.

## Steps to reproduce

Requires only SDL3: no game assets, no shaders.

```
cmake -B build && cmake --build build
./build/poc              # WARNING: expected to lock the GPU
./build/poc --in-bounds  # same code, legal regions: completes normally
```
