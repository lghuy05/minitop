# minitop

A tiny `top`-like process monitor for Linux. It reads `/proc` to show uptime,
load average, total process count, and the top processes by CPU usage.

## Requirements

- Linux with `/proc`
- `gcc` (or another C compiler)
- `make`

## Build

```sh
make
```

This produces the `minitop` binary in the project root.

## Run

```sh
./minitop
```

Press `Ctrl+C` to quit.

## Output

Each refresh shows:

- Uptime and total idle time
- Load averages (1, 5, 15 minutes)
- Total process count
- Top 15 processes by CPU%, with RSS in KB

## Notes

- CPU% is computed from deltas between refreshes (1s interval).
- RSS is parsed from `/proc/<pid>/status` (`VmRSS`).
- This tool is Linux-specific and will not run on macOS or Windows.
