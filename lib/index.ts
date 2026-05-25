/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

import {
    bdev_specs, lxc_attach_options, lxc_clone_options,
    lxc_console_log, LXC_CREATE, LXC_MIGRATE, LXC_MOUNT, lxc_mount, lxc_snapshot, migrate_opts,
    Image, ConsoleSession,
} from "./types";

export * from "./types"

import binding from "./binding"

//region types

export type ContainerState =
    "STOPPED"
    | "STARTING"
    | "RUNNING"
    | "STOPPING"
    | "ABORTING"
    | "FREEZING"
    | "FROZEN"
    | "THAWED";

export type ContainerStats = {
    /** Current memory usage in bytes (cgroup v1: memory.usage_in_bytes) */
    "memory.usage_in_bytes": string | null;
    /** Memory limit in bytes (cgroup v1: memory.limit_in_bytes) */
    "memory.limit_in_bytes": string | null;
    /** Memory+swap usage in bytes (cgroup v1: memory.memsw.usage_in_bytes) */
    "memory.memsw.usage_in_bytes": string | null;
    /** Total CPU time consumed in nanoseconds (cgroup v1: cpuacct.usage) */
    "cpuacct.usage": string | null;
    /** CPU scheduler accounting stats (cgroup v1: cpu.stat) */
    "cpu.stat": string | null;
    /** Block I/O service bytes (cgroup v1: blkio.throttle.io_service_bytes) */
    "blkio.throttle.io_service_bytes": string | null;
};

export type Container = {
    /**
     * Last error number and human-readable description of the last error that occurred.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aad1307b63ded0ac82e7a0adc06969dd8 error_num}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a2fbfcf120528886aec7178e4d3631cba error_string}
     */
    get error(): { num: number, string: string | null }

    /**
     * Name of the container.
     * Getting this property calls `lxc_container.name` directly.
     * Setting this property calls `rename` — the container must be stopped.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a369207c40b4946a04026861bfef3d86c rename}
     */
    get name(): string;
    set name(newName: string);

    /**
     * Determine whether the container configuration file exists (`/var/lib/lxc/$name/config`).
     * Does not indicate whether the container is running.
     * @returns `true` if the container is defined (config exists), `false` otherwise.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a84fd83757b47401b95ff01efae76d896 is_defined}
     */
    get defined(): boolean;

    /**
     * Determine the current state of the container.
     * @returns One of the {@link ContainerState} string values.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aa071116fd2f39c4cbe62d171566d14b9 state}
     */
    get state(): ContainerState;

    /**
     * Determine whether the container is currently running.
     * @returns `true` if running, `false` otherwise.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab810a7cd940111dbb480eb0440f71abe is_running}
     */
    get running(): boolean;

    /**
     * Process ID of the container's init process as seen from outside the container.
     * Returns -1 if the container is not running.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a8ce63dc530f57a3a198ed2d0f0c229a9 init_pid}
     */
    get initPID(): number;

    /**
     * Current configuration file name (full path).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab8e79d0775d59cb0007c7f5e56895a59 config_file_name}
     */
    get configFileName(): string;

    /**
     * Whether the container wishes to be daemonized (detached from the controlling terminal) on start.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aad6d903f5f22937f0662814ee4c1ac05 daemonize}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a9872b6f471de0ccbb5151e692bdbfb3b want_daemonize}
     */
    get daemonize(): boolean;
    set daemonize(value: boolean);

    /**
     * Full path to the container's configuration directory.
     * Each container can have a custom configuration path; by default it is the `lxc.lxcpath`
     * global config value (typically `/var/lib/lxc`).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a3cdd629f0b11a313938178a46b18a263 get_config_path}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ad140523960327ab5537629ee8dc62dd3 set_config_path}
     */
    get configPath(): string;
    set configPath(path: string);

    new(name: string, configPath?: string, alt_file?: string): Container;

    /**
     * Freeze a running container (pause all processes via cgroup freezer).
     * The container must be in the RUNNING state; it will transition to FROZEN.
     * @returns {Promise<void>} Resolves when frozen; rejects on failure.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab83d343ed927520ee693c3c0fc3514ca freeze}
     */
    freeze(): Promise<void>;

    /**
     * Thaw (unfreeze) a frozen container, resuming all paused processes.
     * The container must be in the FROZEN state; it will transition back to RUNNING.
     * @returns {Promise<void>} Resolves when thawed; rejects on failure.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ac1710a6788be63a2429918ec3724641e unfreeze}
     */
    unfreeze(): Promise<void>;

    /**
     * Load the specified configuration file for the container, replacing any previously loaded config.
     * @param alt_file {string} Full path to the alternate configuration file to load.
     * @returns {Promise<void>} Resolves on success; rejects if the file cannot be loaded.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a1568b8bee1175956b9917dcf0458248a load_config}
     */
    loadConfig(alt_file: string): Promise<void>;

    /**
     * Start the container.
     * If the container is already running, this is a no-op.
     * @param useinit {number} Pass `1` to use `lxcinit` rather than `/sbin/init`. Default `0`.
     * @param argv {string[]} Optional arguments to pass to the init process.
     * @returns {Promise<void>} Resolves when the container has started; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a599d630c220ee5ef2a9c6b79c7eb3b23 start}
     */
    start(useinit?: number, argv?: string[]): Promise<void>;

    /**
     * Stop the container by sending it a `SIGKILL`.
     * If the container is already stopped, this is a no-op.
     * @returns {Promise<void>} Resolves when stopped; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a661d604dd5d29b5d6dd5d141d63c8539 stop}
     */
    stop(): Promise<void>;

    /**
     * Set whether all file descriptors should be closed before the container's init process starts.
     * @param state {boolean} `true` to close all fds on startup.
     * @returns {boolean} `true` on success.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a2c979929461fcb4ebe96baed2b0acb8f want_close_all_fds}
     */
    wantCloseAllFds(state: boolean): Promise<void>;

    /**
     * Block until the container reaches the specified state or the timeout expires.
     * @param state {ContainerState} The target state to wait for.
     * @param timeout {number} Seconds to wait; `-1` waits forever, `0` returns immediately. Default `-1`.
     * @returns {Promise<void>} Resolves when the state is reached; rejects on timeout or error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a103d932d1b2ba4e8b07100ef4dbd053f wait}
     */
    wait(state: ContainerState, timeout?: number): Promise<void>;

    /**
     * Set a single key/value pair in the container's in-memory configuration.
     * Call `save()` afterwards to persist the change to disk.
     * @param key {string} Configuration key (e.g. `"lxc.log.file"`).
     * @param value {string} Value to set.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a340914109541781444138be66b4e88eb set_config_item}
     */
    setConfigItem(key: string, value: string): void;

    /**
     * Delete the container and optionally its snapshots.
     * The container must be stopped before calling this unless `force` is set.
     * Internally calls `destroy_with_snapshots` when `include_snapshots` is `true`.
     * @param options.include_snapshots {boolean} Destroy all snapshots too. Default `false`.
     * @param options.force {boolean} Stop the container first if it is running. Default `false`.
     * @returns {Promise<void>} Resolves on success; rejects if destruction fails.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a284790c42e39e597eae61ae2d2995173 destroy}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#af25cbca9e062b5fc9279753361391b85 destroy_with_snapshots}
     */
    destroy(options?: {
        include_snapshots?: boolean,
        force?: boolean
    }): Promise<void>

    /**
     * Persist the container's in-memory configuration to a file.
     * @param alt_file {string} Full path to the file to write. Pass the current `configFileName`
     *   to overwrite the existing config.
     * @returns {Promise<void>} Resolves on success; rejects if the file cannot be written.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#abea85657b8f816e7d2d5238c0d731a67 save_config}
     */
    save(alt_file: string): Promise<void>;

    /**
     * Create a new container using a template or a pre-built image.
     * This runs the template script (e.g. `download`, `ubuntu`) to populate the rootfs.
     * @param options.template {string} Template name to use (e.g. `"download"`, `"ubuntu"`). Default `"none"`.
     * @param options.argv {string[]} Arguments to pass to the template script (e.g. distro/release/arch).
     * @param options.bdevtype {string} Backing store type (`"dir"`, `"lvm"`, `"zfs"`, `"rbd"`, etc.). Default `"dir"`.
     * @param options.bdev_specs {Partial<bdev_specs>} Backing store parameters (LVM VG, ZFS root, etc.).
     * @param options.flags {LXC_CREATE} Creation flags (e.g. `LXC_CREATE.QUIET`).
     * @returns {Promise<void>} Resolves when the container has been created; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ad2a402be12791e43dd016bba24e53b40 create}
     */
    create(options: {
        template?: string,
        argv?: string[]
        bdevtype?: string,
        bdev_specs?: Partial<bdev_specs>,
        flags?: LXC_CREATE
    }): Promise<void>;

    /**
     * Request the container to reboot by sending it `SIGINT` to its init process.
     * Waits up to `timeout` seconds for the reboot to complete.
     * Internally uses `reboot2` which supports a timeout parameter.
     * @param timeout {number} Seconds to wait; `-1` waits forever, `0` does not wait. Default `-1`.
     * @returns {Promise<boolean>} Resolves with `true` if the reboot completed within the timeout.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a5c6f030b9e8797cf9a4586b1fab6c3eb reboot2}
     */
    reboot(timeout?: number): Promise<boolean>;

    /**
     * Request the container to shut down gracefully by sending `SIGPWR` to its init process.
     * Waits up to `timeout` seconds for the shutdown to complete.
     * @param timeout {number} Seconds to wait; `-1` waits forever, `0` does not wait. Default `-1`.
     * @returns {Promise<boolean>} Resolves with `true` if shutdown completed within the timeout.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a9cfba9429c159612a8d6b768f6e27ade shutdown}
     */
    shutdown(timeout?: number): Promise<boolean>;

    /**
     * Completely clear the container's in-memory configuration.
     * Does not affect the configuration file on disk. Call `loadConfig` to reload from disk.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a2d40919280dbedd9d29417d5ca117e86 clear_config}
     */
    clearConfig(): void;

    /**
     * Clear a single key from the container's in-memory configuration.
     * @param key {string} Name of the configuration key to remove (e.g. `"lxc.net.0.type"`).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a4521fb10eee10cf6ca9b33d3ef1f996a clear_config_item}
     */
    clearConfigItem(key: string): void;

    /**
     * Retrieve the value of a configuration key from the container's in-memory config.
     * Returns `null` if the key does not exist or has no value.
     * @param key {string} Configuration key to retrieve (e.g. `"lxc.net.0.type"`).
     * @returns {string | null} The value string, or `null` if absent.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a7411db2a0e7a8471c41a4a58d8e83b33 get_config_item}
     */
    getConfigItem(key: string): string | null;

    /**
     * Retrieve the value of a configuration key from a **running** container's live state.
     * Unlike `getConfigItem`, this reads values that may differ from the on-disk config
     * (e.g. effective network settings, runtime cgroup values).
     * Returns `null` if the key does not exist or the container is not running.
     * @param key {string} Configuration key to retrieve.
     * @returns {string | null} The live value string, or `null` if absent.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aeb0f5f1589ba5dc14d5d1e1c0cb87f96 get_running_config_item}
     */
    getRunningConfigItem(key: string): string | null;

    /**
     * Retrieve all configuration keys that match the given prefix.
     * Use this to enumerate available keys, then call `getConfigItem` on each.
     * For bulk reads, prefer `getConfigItems` which fetches keys and values in one call.
     * @param prefix {string} Key prefix to filter by (e.g. `"lxc.net"`). Pass `undefined` for all keys.
     * @returns {string[]} Array of matching key strings.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a885bbd0d02c3771bcee7b02a2c398be9 get_keys}
     */
    getKeys(prefix?: string): string[]

    /**
     * Retrieve all network interfaces currently visible inside the container.
     * The container must be running.
     * @returns {Promise<string[]>} Array of interface name strings (e.g. `["lo", "eth0"]`).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a50d09300fa69182156b58eb9ef288dbd get_interfaces}
     */
    getInterfaces(): Promise<string[]>

    /**
     * Retrieve IPv4 addresses assigned to a container interface.
     * The container must be running.
     * @param iface {string} Network interface name (e.g. `"eth0"`).
     * @param family {"inet"} Address family — use `"inet"` for IPv4.
     * @returns {Promise<string[]>} Array of IPv4 address strings in dotted-decimal notation.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a70b47b5719463e4c61ed097b285d9371 get_ips}
     */
    getIPs(iface: string, family: "inet"): Promise<string[]>

    /**
     * Retrieve IPv6 addresses assigned to a container interface.
     * The container must be running.
     * @param iface {string} Network interface name (e.g. `"eth0"`).
     * @param family {"inet6"} Address family — use `"inet6"` for IPv6.
     * @param scope {number} IPv6 scope ID to filter by (e.g. `0` for global).
     * @returns {Promise<string[]>} Array of IPv6 address strings.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a70b47b5719463e4c61ed097b285d9371 get_ips}
     */
    getIPs(iface: string, family: "inet6", scope: number): Promise<string[]>

    /**
     * Retrieve the value of a cgroup subsystem entry for the container.
     * Returns `undefined` if the subsystem does not exist or is not available.
     * @param subsys {string} cgroup subsystem path (e.g. `"memory.usage_in_bytes"`, `"cpu.stat"`).
     * @returns {string | undefined} The cgroup value string, or `undefined`.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a6202bf97cd77e208eed1845d5a911cbc get_cgroup_item}
     */
    getCGroupItem(subsys: string): string | undefined;

    /**
     * Set a cgroup subsystem value for the container.
     * The container must be running for most cgroup writes to take effect.
     * @param subsys {string} cgroup subsystem path (e.g. `"memory.limit_in_bytes"`).
     * @param value {string} Value to write (e.g. `"536870912"` for 512 MiB).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aff7753133cff04ed1e48fdceaf490a28 set_cgroup_item}
     */
    setCGroupItem(subsys: string, value: string): void;

    /**
     * Clone (copy) a stopped container into a new container.
     * The source container must be stopped.
     * @param options {lxc_clone_options} Clone parameters including new name, backing store, and flags.
     * @returns {Promise<Container>} Resolves with a Container instance for the new clone.
     * @note If `bdevtype` is not specified and `LXC_CLONE_SNAPSHOT` is set in flags, the native
     *   backing store snapshot mechanism is used if available, otherwise overlayfs is used.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ada03abc45ce41a3229837858a25de841 clone}
     */
    clone(options: lxc_clone_options): Promise<Container>;

    /**
     * Allocate a console TTY file descriptor pair for the container without attaching to it.
     * The returned `ttyfd` keeps the TTY allocated; close both descriptors when done.
     * @param ttynum {number} TTY number to allocate, or `-1` for the first available. Default `-1`.
     * @returns {Promise<[number, number]>} Tuple of `[ttyfd, ptxfd]` — the TTY and pseudoterminal master fds.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a64b4ce75b807b21e42d4ccd024652973 console_getfd}
     */
    consoleGetFds(ttynum?: number): Promise<[number, number]>

    /**
     * Allocate a console TTY and return an async event-emitting session object.
     * The returned {@link ConsoleSession} emits `data` events with `Buffer` chunks and a `close` event
     * when the session ends. Supports `write(data)`, `resize(cols, rows)`, and `close()`.
     * This is a custom enhancement not directly in the LXC C API.
     * @param ttynum {number} TTY number to allocate, or `-1` for the first available. Default `-1`.
     * @returns {Promise<ConsoleSession>} Active console session.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a64b4ce75b807b21e42d4ccd024652973 console_getfd}
     */
    consoleAsync(ttynum?: number): Promise<ConsoleSession>

    /**
     * Allocate and run a console TTY, blocking until the user exits.
     * This connects the calling process's stdio to the container console.
     * Use `consoleAsync` instead for non-blocking programmatic access.
     * @param ttynum {number} TTY number to allocate (`-1` = first available, `0` = console).
     * @param stdio {[number, number, number]} File descriptors `[stdinfd, stdoutfd, stderrfd]` to use.
     * @param escape {number} Escape character as integer (`1` = Ctrl+a, `2` = Ctrl+b, …).
     * @returns {Promise<void>} Resolves when the user exits the console.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a5ef781838651c315c8cdc6730586a554 console}
     */
    console(ttynum: number, stdio: [number, number, number], escape: number): Promise<void>;

    /**
     * Open a shell inside the container and wait for it to exit.
     * Uses `lxc_attach_run_shell` internally, which spawns `/bin/sh` (or the container's default shell).
     * The shell inherits the attach options (namespaces, environment, uid/gid, etc.).
     * @param options {Partial<lxc_attach_options>} Attach options. Uses LXC defaults if omitted.
     * @returns {Promise<number>} Exit code of the shell process.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a1fd6ce7695e6a967efbb9dcc36952168 attach}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html lxc_attach_options_t}
     */
    attach(options?: Partial<lxc_attach_options>): Promise<number>;

    /**
     * Create a container snapshot at the current state.
     * Snapshots are stored as `snap<N>` under the container's snapshot directory
     * (typically `/var/lib/lxc/<name>/snaps/snap<N>`).
     * @param commentfile {string} Full path to a file containing a human-readable description of the snapshot.
     * @returns {Promise<number>} The zero-based snapshot number assigned (e.g. `0` for `snap0`).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab81bbbabd39179066c4bca619e3bcb7d snapshot}
     */
    snapshot(commentfile: string): Promise<number>;

    /**
     * List all snapshots for this container.
     * @returns {Promise<lxc_snapshot[]>} Array of snapshot descriptor objects.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aba8a7bc21d7a805110563f5446d9fe82 snapshot_list}
     */
    snapshotList(): Promise<lxc_snapshot[]>

    /**
     * Restore a container from a named snapshot.
     * The restored container is a full copy of the snapshot placed at `newname` in the same lxcpath.
     * @param snapname {string} Snapshot name (e.g. `"snap0"`).
     * @param newname {string} Name for the restored container. Defaults to the current container's name,
     *   which replaces the container in-place (fails for overlay-based snapshots that pin the original).
     * @returns {Promise<void>} Resolves when the restore is complete.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#acc0a13de063830a2ccee54ff88f57f79 snapshot_restore}
     */
    snapshotRestore(snapname: string, newname?: string): Promise<void>

    /**
     * Destroy a single named snapshot.
     * @param snapname {string} Name of the snapshot to destroy (e.g. `"snap0"`).
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a80009a1f8c81e8c0776e9e05d18f0d6b snapshot_destroy}
     */
    snapshotDestroy(snapname: string): Promise<void>

    /**
     * Destroy all snapshots for this container.
     * @param all {true} Must be `true`.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a16c2f4e7f308f8b1c0b281145cc9c845 snapshot_destroy_all}
     */
    snapshotDestroy(all: true): Promise<void>

    /**
     * Add a host device node into a running container.
     * @param src_path {string} Full path of the device on the host (e.g. `"/dev/sdb"`).
     * @param dest_path {string | undefined} Path inside the container. Defaults to `src_path`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a269156192f005e5f3cd44f5834637023 add_device_node}
     */
    addDeviceNode(src_path: string, dest_path?: string): Promise<void>

    /**
     * Remove a device node from a running container.
     * @param src_path {string} Full path of the device on the host.
     * @param dest_path {string | undefined} Path inside the container. Defaults to `src_path`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a3866a5cc79af7e8d8d6b15055f356f73 remove_device_node}
     */
    removeDeviceNode(src_path: string, dest_path?: string): Promise<void>

    /**
     * Move a host network interface into a running container.
     * @param dev {string} Name of the network device on the host (e.g. `"eth1"`).
     * @param dst_dev {string | undefined} Name to use inside the container. Defaults to `dev`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a33aadfb28b145095bb7213b6bfb02b57 attach_interface}
     */
    attachInterface(dev: string, dst_dev?: string): Promise<void>

    /**
     * Remove a network interface from a running container and return it to the host.
     * @param dev {string} Name of the network device inside the container.
     * @param dst_dev {string | undefined} Name to restore on the host. Defaults to `dev`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ac00d4ee3286ca5c0f2d4283c283fc0ec detach_interface}
     */
    detachInterface(dev: string, dst_dev?: string): Promise<void>

    /**
     * Checkpoint a running container using CRIU. The container state is written to `directory`
     * and the container can optionally be stopped afterwards.
     * @param directory {string} Directory path to write the checkpoint images to.
     * @param stop {boolean} Stop the container after checkpointing. Default `false`.
     * @param verbose {boolean} Enable verbose CRIU logging. Default `false`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a0a034333ca81e2a80b5e52bfadd73c5b checkpoint}
     */
    checkpoint(directory: string, stop?: boolean, verbose?: boolean): Promise<void>

    /**
     * Restore a container from a CRIU checkpoint directory.
     * @param directory {string} Directory path containing the CRIU checkpoint images.
     * @param verbose {boolean} Enable verbose CRIU logging. Default `false`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ac7c05a5b4bd5e85557dc080251da2db1 restore}
     */
    restore(directory: string, verbose?: boolean): Promise<void>

    /**
     * Perform a migration operation (dump, restore, or pre-dump) using CRIU.
     * Use the `LXC_MIGRATE` enum values for `cmd`.
     * @param cmd {LXC_MIGRATE} Operation to perform (`MIGRATE_DUMP`, `MIGRATE_RESTORE`, or `MIGRATE_PRE_DUMP`).
     * @param options {Partial<migrate_opts>} Migration options including directory, page server address, etc.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aaa8cbcb64d51efb940789863d22e9572 migrate}
     */
    migrate(cmd: LXC_MIGRATE, options?: Partial<migrate_opts>): Promise<void>

    /**
     * Query or clear the container's in-kernel console ring buffer log.
     * @param options {Partial<lxc_console_log>} Options controlling whether to read, clear, and how many bytes to return.
     * @returns {Promise<string>} The console log data as a string.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a1d9a42547c16a3350e47e9b8978692c1 console_log}
     */
    consoleLog(options?: Partial<lxc_console_log>): Promise<string>

    /**
     * Bind-mount a host path into the running container.
     * Equivalent to calling `mount(2)` inside the container's mount namespace.
     * @param source {string} Host filesystem path to mount.
     * @param target {string} Mount point inside the container.
     * @param filesystemtype {string | undefined} Filesystem type (e.g. `"tmpfs"`), or `undefined` for bind mounts.
     * @param mountflags {bigint | number | LXC_MOUNT} Flags passed to `mount(2)` (e.g. `MS_BIND`, `MS_RDONLY`).
     * @param mnt {lxc_mount} Mount API version descriptor. Set `version` to `0`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a6a28265676c212bfe25dbe0c1f7e30ac mount}
     */
    mount(source: string, target: string, filesystemtype: string | undefined, mountflags: bigint | number | LXC_MOUNT, mnt: lxc_mount): Promise<void>

    /**
     * Unmount a path inside the running container.
     * @param source {string} Path inside the container to unmount.
     * @param mountflags {bigint | number} Flags passed to `umount2(2)` (e.g. `MNT_DETACH`).
     * @param mnt {lxc_mount} Mount API version descriptor. Set `version` to `0`.
     * @returns {Promise<void>} Resolves on success; rejects on error.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a57af1263431950dc4666ba75eda7a817 umount}
     */
    umount(source: string, mountflags: bigint | number, mnt: lxc_mount): Promise<void>

    /**
     * Retrieve a file descriptor for the container's seccomp notification filter.
     * Used to receive seccomp notifications from the container via `poll(2)`.
     * The container does not need to be running; the fd is pre-loaded at container start.
     * @returns {Promise<number>} The seccomp notification fd.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aa94c6d2e4f992a9323988e16c05215c2 seccomp_notify_fd}
     */
    seccompNotifyFd(): Promise<number>

    /**
     * Retrieve a file descriptor for the **running** container's active seccomp filter.
     * Unlike `seccompNotifyFd`, this reflects the live filter of a running container.
     * @returns {Promise<number>} The active seccomp notification fd.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a0247dee4857aa62cff8e76d007e33848 seccomp_notify_fd_active}
     */
    seccompNotifyFdActive(): Promise<number>

    /**
     * Retrieve a pidfd for the container's init process.
     * A pidfd is a file descriptor that refers to a process and is immune to PID reuse.
     * Useful for safely signalling or polling the container's init with `waitid(P_PIDFD)`.
     * @returns {Promise<number>} The pidfd of the container's init process.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a8cdfb90d01607eb81405ee09504bc7e9 init_pidfd}
     */
    initPIDFd(): Promise<number>

    /**
     * Retrieve a mount fd for the container's devpts instance.
     * This fd can be used to access the container's pseudo-terminal device hierarchy.
     * @returns {Promise<number>} The mount fd for the container's devpts.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a863a304c9c98e570b016d59ec2016725 devpts_fd}
     */
    devptsFd(): Promise<number>

    /**
     * Run a command inside the container and wait for it to exit.
     * Uses `lxc_attach_run_command` internally. `argv[0]` is the program to run;
     * subsequent entries are its arguments.
     * @param options.argv {string[]} Command and arguments (e.g. `["/bin/ls", "-la", "/tmp"]`).
     * @param options {Partial<lxc_attach_options>} Additional attach options (uid, gid, env, namespaces, etc.).
     * @returns {Promise<number>} Exit code of the command that ran inside the container.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a8a038faa0449ef8206311e8d5f47f370 attach_run_wait}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html lxc_attach_options_t}
     */
    exec(options: Partial<lxc_attach_options> & { argv: string[] }): Promise<number>;

    /**
     * Set a global timeout for LXC operations on this container.
     * After the timeout expires LXC will abort the operation with an error.
     * Pass `-1` to disable the timeout (wait indefinitely).
     * @param timeout {number} Timeout in seconds, or `-1` to disable.
     * @returns {boolean} `true` on success.
     * @note Requires LXC ≥ 4.0.12. Throws if the library version is older.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h set_timeout}
     */
    setTimeout(timeout: number): boolean;

    /**
     * Determine whether the calling process has sufficient privileges to control this container.
     * Useful as a pre-flight check before attempting operations that require control privileges.
     * @returns {boolean} `true` if the caller may control the container.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a77091f6f92b534595602e433b717f5cd may_control}
     */
    mayControl(): boolean;

    /**
     * Retrieve all configuration keys and their values in a single call.
     * More efficient than calling `getKeys(prefix)` followed by N `getConfigItem(key)` calls.
     * @param prefix {string | undefined} Optional key prefix to filter by (e.g. `"lxc.net"`).
     *   Pass `undefined` to retrieve all keys.
     * @returns {Record<string, string | null>} Object mapping each key to its value, or `null` if
     *   the key exists but has no value.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a885bbd0d02c3771bcee7b02a2c398be9 get_keys}
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a7411db2a0e7a8471c41a4a58d8e83b33 get_config_item}
     */
    getConfigItems(prefix?: string): Record<string, string | null>;

    /**
     * Retrieve a snapshot of common resource usage metrics for a running container.
     * Reads six cgroup v1 entries in a single async operation. Returns `null` for
     * any metric not available on the host kernel or cgroup hierarchy.
     * @returns {Promise<ContainerStats>} Object with cgroup metric values.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a6202bf97cd77e208eed1845d5a911cbc get_cgroup_item}
     */
    stats(): Promise<ContainerStats>;
}


//endregion

export type LXC = {
    /**
     * Return the LXC library version string.
     * @returns {string} Version string (e.g. `"5.0.3"`).
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h lxc_get_version}
     */
    GetVersion(): string,
    /**
     * Retrieve a global LXC configuration item.
     * Common keys include `"lxc.lxcpath"` (container directory) and `"lxc.default_config"`.
     * @param key {string} Global configuration key to retrieve.
     * @returns {string} The value of the global configuration item.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h lxc_get_global_config_item}
     */
    GetGlobalConfigItem(key: string): string,
    /**
     * List the names of all defined and active containers.
     * Equivalent to `list_defined_containers` ∪ `list_active_containers`.
     * @param lxcpath {string | undefined} Container directory to search. Defaults to `lxc.lxcpath`.
     * @returns {string[]} Array of container name strings.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h list_all_containers}
     */
    ListAllContainers(lxcpath?: string): string[],
    /**
     * List the names of all defined (configured but not necessarily running) containers.
     * A container is defined if its configuration file exists.
     * @param lxcpath {string | undefined} Container directory to search. Defaults to `lxc.lxcpath`.
     * @returns {string[]} Array of container name strings.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h list_defined_containers}
     */
    ListAllDefinedContainers(lxcpath?: string): string[],
    /**
     * List the names of all currently active (running) containers.
     * @param lxcpath {string | undefined} Container directory to search. Defaults to `lxc.lxcpath`.
     * @returns {string[]} Array of container name strings.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h list_active_containers}
     */
    ListAllActiveContainers(lxcpath?: string): string[],
    /**
     * Determine whether a specific configuration key is supported by this LXC build.
     * Useful before setting keys that may not exist in older LXC versions.
     * @param key {string} Configuration key to test (e.g. `"lxc.cap.drop"`).
     * @returns {boolean} `true` if the key is supported, `false` otherwise.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h lxc_config_item_is_supported}
     */
    ConfigItemIsSupported(key: string): boolean,
    /**
     * Determine whether a specific LXC API extension is available in this build.
     * Extension names are stable strings such as `"container_snapshot_comments"`,
     * `"mount_injection"`, or `"seccomp_notify"`.
     * @param extension {string} Extension name to test.
     * @returns {boolean} `true` if the extension is available, `false` otherwise.
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h lxc_has_api_extension}
     */
    HasApiExtension(extension: string): boolean,
    /**
     * Return the list of valid container wait-state strings.
     * These are the values accepted by `container.wait()` and returned by `container.state`.
     * @returns {string[]} Array of state name strings (e.g. `["STOPPED", "STARTING", "RUNNING", ...]`).
     * @see {@link https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h lxc_get_wait_states}
     */
    GetWaitStates(): string[],
    /**
     * Container class constructor. Create a new handle for an LXC container by name.
     * @param name {string} Name of the container.
     * @param configPath {string | undefined} Path to the LXC container directory. Defaults to `lxc.lxcpath`.
     * @param alt_file {string | undefined} Path to an alternate configuration file to load.
     * @see {@link https://linuxcontainers.org/lxc/apidoc/structlxc__container.html lxc_container}
     */
    Container: Container
}

export const LXC: LXC = binding;

export const {
    GetVersion,
    GetGlobalConfigItem,
    ListAllContainers,
    ListAllDefinedContainers,
    ListAllActiveContainers,
    ConfigItemIsSupported,
    HasApiExtension,
    GetWaitStates,
    Container,
} = LXC

export const Images = {

    repositories: {
        linuxcontainers: {
            'base.url': 'https://images.linuxcontainers.org',
            'image.json': "https://images.linuxcontainers.org/meta/simplestreams/v1/images.json",
            'index.json': "https://images.linuxcontainers.org/meta/simplestreams/v1/index.json",
        }
    } as const,

    async List(repository: keyof typeof Images.repositories = 'linuxcontainers'): Promise<Record<string, Image>> {
        const imageURL = Images.repositories[repository]["image.json"];
        const response = await fetch(imageURL);
        if (!response.ok) throw new Error(`Failed to get image list from '${imageURL}'`, {cause: response});
        const images = await response.json();
        if (images?.content_id !== "images") throw new Error("Unknown json: content_id !== 'images'")
        if (images?.datatype !== "image-downloads") throw new Error("Unknown json: datatype !== 'image-downloads'")
        if (images?.format !== "products:1.0") throw new Error("Unknown json: format !== 'products:1.0'")
        if (!images.products || typeof images.products !== 'object') throw new Error("Unknown json: products is not defined or is not of type object");
        return images.products as Record<string, Image>;
    },

    async Available(repository: keyof typeof Images.repositories = 'linuxcontainers'): Promise<string[]> {
        const indexURL = Images.repositories[repository]["index.json"];
        const response = await fetch(indexURL);
        if (!response.ok) throw new Error(`Failed to get image index list from '${indexURL}'`, {cause: response});
        const images = await response.json();
        if (!images?.index) throw new Error("Unknown json: index is undefined")
        if (!images?.index?.images || typeof images.index.images !== 'object') throw new Error("Unknown json: typeof index.images !== object")
        if (images.index.images?.datatype !== "image-downloads") throw new Error("Unknown json: index.images.datatype !== 'image-downloads'")
        if (images.index.images?.format !== "products:1.0") throw new Error("Unknown json: index.images.format !== 'products:1.0'")
        if (!images.index.images?.products || !Array.isArray(images.index.images.products)) throw new Error("Unknown json: images.index.images.products is not defined or is not of type string[]");
        return images.index.images?.products as string[];
    }
}


export * from "./types"
