# node-lxc

> Node.js native bindings for [LXC](https://linuxcontainers.org/lxc/) (Linux Containers) — a complete, production-ready wrapper around `liblxc` built with N-API.

[![npm version](https://img.shields.io/npm/v/node-lxc.svg)](https://www.npmjs.com/package/node-lxc)
[![license](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Node.js ≥ 12](https://img.shields.io/badge/node-%3E%3D12-brightgreen.svg)](https://nodejs.org)
[![Platform: Linux](https://img.shields.io/badge/platform-linux-blue.svg)](https://linuxcontainers.org)

## Features

- **Full lifecycle control** — create, start, stop, reboot, shutdown, destroy
- **Async-first** — all blocking operations return `Promise`s backed by a libuv thread pool
- **Interactive console** — non-blocking console sessions with `EventEmitter` interface
- **Rich configuration API** — get, set, clear, and bulk-read container configuration
- **Snapshot support** — create, list, restore, and destroy snapshots
- **CRIU integration** — checkpoint, restore, and live-migrate containers
- **Mount injection** — bind-mount paths into running containers without restart
- **Resource metrics** — async cgroup stat reads (memory, CPU, block I/O)
- **Network management** — attach/detach interfaces, query IPs and interfaces
- **Device hotplug** — add/remove device nodes into running containers
- **Seccomp support** — retrieve seccomp notification file descriptors
- **Memory-safe** — no leaks; all heap buffers freed, all async captures by value
- **TypeScript** — full type declarations included

## Prerequisites

| Requirement | Version |
|---|---|
| Node.js | ≥ 12.17 |
| LXC | ≥ 4.0 |
| liblxc-dev | (same as LXC) |
| g++ | ≥ 7 |

Install LXC on Debian/Ubuntu:

```sh
sudo apt install lxc lxc-dev
```

Verify your kernel supports LXC features:

```sh
lxc-checkconfig
```

## Installation

```sh
npm install node-lxc
```

The package ships a pre-built binary for `x86_64-linux-gnu`. No compilation step is required for this platform.

## Quick Start

```typescript
import { Container, GetVersion } from "node-lxc";

console.log("LXC", GetVersion()); // e.g. "5.0.2"

// Create and start a container
const c = new Container("my-container");

await c.create({
    template: "download",
    argv: ["--dist", "ubuntu", "--release", "jammy", "--arch", "amd64"],
});

c.setConfigItem("lxc.net.0.type", "veth");
c.setConfigItem("lxc.net.0.link", "lxcbr0");
c.setConfigItem("lxc.net.0.flags", "up");

await c.start();
console.log(c.state);   // "RUNNING"
console.log(c.initPID); // e.g. 12345

// Run a command and get the exit code
const code = await c.exec({ argv: ["/bin/sh", "-c", "echo hello"] });
console.log(code); // 0

await c.shutdown(30);
await c.destroy();
```

## API

### Global functions

```typescript
import {
    GetVersion,               // → string
    GetGlobalConfigItem,      // (key: string) → string
    ListAllContainers,        // (lxcpath?: string) → string[]
    ListAllDefinedContainers, // (lxcpath?: string) → string[]
    ListAllActiveContainers,  // (lxcpath?: string) → string[]
    ConfigItemIsSupported,    // (key: string) → boolean
    HasApiExtension,          // (extension: string) → boolean
    GetWaitStates,            // () → string[]
} from "node-lxc";
```

### Container constructor

```typescript
const c = new Container(name: string, configPath?: string, alt_file?: string);
```

| Parameter | Description |
|---|---|
| `name` | Container name |
| `configPath` | LXC container directory (defaults to `lxc.lxcpath`) |
| `alt_file` | Alternate configuration file path |

### Lifecycle

```typescript
await c.create({ template?, argv?, bdevtype?, bdev_specs?, flags? });
await c.start(useinit?, argv?);
await c.stop();
await c.reboot(timeout?);          // → Promise<boolean>
await c.shutdown(timeout?);        // → Promise<boolean>
await c.freeze();
await c.unfreeze();
await c.destroy({ include_snapshots?, force? });
await c.clone(options);            // → Promise<Container>
```

### State and introspection

```typescript
c.name                   // string
c.defined                // boolean
c.running                // boolean
c.state                  // ContainerState ("RUNNING" | "STOPPED" | ...)
c.initPID                // number (-1 if not running)
c.error                  // { num: number, string: string | null }
c.daemonize              // boolean (get/set)
c.configPath             // string (get/set)
c.configFileName         // string
c.mayControl()           // → boolean
await c.initPIDFd()      // → number (pidfd)
await c.devptsFd()       // → number
```

### Configuration

```typescript
c.getConfigItem(key)              // → string | null
c.setConfigItem(key, value)
c.clearConfigItem(key)
c.clearConfig()
c.getRunningConfigItem(key)       // → string | null (live, running container)
c.getKeys(prefix?)                // → string[]
c.getConfigItems(prefix?)         // → Record<string, string | null>
await c.loadConfig(alt_file)
await c.save(alt_file)
```

### Execution and console

```typescript
// Run a command; returns its exit code
await c.exec({ argv: string[], ...lxc_attach_options })  // → number

// Open a shell; returns the shell's exit code
await c.attach(options?)                                  // → number

// Non-blocking async console session (EventEmitter)
const session = await c.consoleAsync(ttynum?)  // → ConsoleSession
session.on("data", (chunk: Buffer) => { ... });
session.on("close", () => { ... });
session.write(data);
session.resize(cols, rows);
session.close();

// Blocking console (connects current stdio)
await c.console(ttynum, [stdinfd, stdoutfd, stderrfd], escape);

// Raw TTY allocation
const [ttyfd, ptxfd] = await c.consoleGetFds(ttynum?);
```

### Resource metrics

```typescript
const stats: ContainerStats = await c.stats();
// {
//   "memory.usage_in_bytes":             string | null,
//   "memory.limit_in_bytes":             string | null,
//   "memory.memsw.usage_in_bytes":       string | null,
//   "cpuacct.usage":                     string | null,
//   "cpu.stat":                          string | null,
//   "blkio.throttle.io_service_bytes":   string | null,
// }
```

### Network

```typescript
await c.getInterfaces()                              // → string[]
await c.getIPs(iface, "inet")                        // → string[] (IPv4)
await c.getIPs(iface, "inet6", scope)                // → string[] (IPv6)
await c.attachInterface(dev, dst_dev?)
await c.detachInterface(dev, dst_dev?)
```

### Devices and cgroups

```typescript
await c.addDeviceNode(src_path, dest_path?)
await c.removeDeviceNode(src_path, dest_path?)
c.getCGroupItem(subsys)              // → string | undefined
c.setCGroupItem(subsys, value)
```

### Snapshots

```typescript
await c.snapshot(commentfile)                      // → number (snap index)
await c.snapshotList()                             // → lxc_snapshot[]
await c.snapshotRestore(snapname, newname?)
await c.snapshotDestroy(snapname)
await c.snapshotDestroy(true)                      // destroy all
```

### Mount injection (running container)

```typescript
await c.mount(source, target, filesystemtype, mountflags, mnt)
await c.umount(source, mountflags, mnt)
```

### CRIU checkpoint / restore / migrate

```typescript
await c.checkpoint(directory, stop?, verbose?)
await c.restore(directory, verbose?)
await c.migrate(cmd: LXC_MIGRATE, options?)
```

### Seccomp

```typescript
await c.seccompNotifyFd()        // → number
await c.seccompNotifyFdActive()  // → number
```

### Console ring buffer

```typescript
await c.consoleLog({ clear?, read?, read_max? })  // → string
```

## Examples

The `examples/` directory contains runnable scripts for common use cases:

| Script | Description |
|---|---|
| `examples/create/` | Create a container from a template |
| `examples/start/` | Start a container |
| `examples/execute/` | Run a command in a container |
| `examples/attach/` | Open a shell in a container |
| `examples/console_async/` | Non-blocking console session |
| `examples/clone/` | Clone a container |
| `examples/checkpoint/` | CRIU checkpoint/restore |
| `examples/concurrent_create/` | Concurrent container creation |
| `examples/stats/` | Read cgroup resource metrics |
| `examples/images/` | Browse available LXC images |

Run any example with ts-node:

```sh
npm run example:create
npm run example:execute
npm run example:console_async
```

## Building from Source

```sh
# Install system dependencies (Debian/Ubuntu)
sudo apt install g++ lxc lxc-dev

# Install npm dependencies
npm install

# Configure and build the native addon
node-gyp configure
node-gyp build

# Compile TypeScript
npx tsc --build
```

## Testing

```sh
npm test
```

The test suite uses Node.js's built-in `node:test` runner. Tests that require root access (container create/start/stop/destroy) are automatically skipped unless the process is running as root.

To run the full integration test suite:

```sh
sudo npm test
```

## Contributing

Bug reports and pull requests are welcome on [GitHub](https://github.com/SourceRegistry/node-lxc).

Please ensure:
- New C++ code uses `std::unique_ptr<char[]>` for heap buffers
- Async lambda captures use by-value (`[this, var]` not `[this, &var]`)
- All public API changes include TypeScript type updates and JSDoc

## License

[Apache 2.0](LICENSE) © 2026 [ProjectSource V.O.F.](https://projectsource.nl)

## See Also

- [LXC API documentation](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html)
- [LXC C header (GitHub)](https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h)
- [Linux Containers](https://linuxcontainers.org)
