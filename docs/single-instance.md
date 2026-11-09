# Single-instance guard

@Opus5.5, 2026-11-09

Running `sioyek <file>` or `sioyek --execute-command ...` while sioyek is
already open does not start a new window: the new process forwards its
arguments to the running one (the *primary instance*) over a local socket and
exits. `pdf_viewer/RunGuard.cpp` decides which process is the primary.

## How it works

- The primary is whichever process holds the lock file `ipc-<hash>.lock`
  (a `QLockFile`). `<hash>` is derived from the instance name (`sioyek`, or
  whatever `--instance-name` gives).
- A lock counts as stale only when the process that wrote it is dead, never
  because of its age. A crash leaves nothing that blocks later launches.
- The primary listens on `ipc-<hash>.sock` next to the lock file.
- Both files live in `~/Library/Application Support/sioyek/` on macOS and in
  `$XDG_RUNTIME_DIR/sioyek/` on Linux. On Windows, the socket is a named pipe
  and the lock file sits in the temp directory.
- A secondary retries connecting for up to 5 seconds, which covers a launch
  that races a primary that holds the lock but is not listening yet.

## Why not shared memory (the old design)

The original guard used `QSharedMemory`. On macOS this is a System V segment
whose key comes from `ftok()` on `$TMPDIR/qipc_sharedmemory_<hash>`. macOS
deletes `$TMPDIR` files that have not been accessed for about three days, and
nothing touches that file while sioyek runs. After it was deleted, the next
launch got a different key, became a second primary and took over the socket.
The old window stayed open but could no longer receive anything, so
`sioyek-reload` after a LaTeX compile went to the wrong window. Seen on
2026-11-09, with `ipcs -m` showing two live segments with different keys.

Upstream PR: https://github.com/ahrm/sioyek/pull/1718 (also relevant to
upstream issues #1089 and #1403, which are about orphaned segments).

## After upgrading

A new binary cannot find an old binary that is still running, because they
use different socket locations. Quit every running sioyek once after
installing a build with this change.

## Diagnosing

```
ls -la ~/"Library/Application Support/sioyek/" | grep ipc-
lsof -U | grep sioyek
```

The `.lock` file holds the primary's PID on its first line. Leftover System V
segments from the old design show up in `ipcs -m` with owner you and
`NATTCH 0`; they are harmless and vanish at reboot (`ipcrm -m <id>` removes
one now).
