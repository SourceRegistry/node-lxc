/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

import {
    bdev_specs, lxc_attach_options, lxc_clone_options,
    lxc_console_log, LXC_CREATE, LXC_MIGRATE, LXC_MOUNT, lxc_mount, lxc_snapshot, migrate_opts,
    Image,
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

export type Container = {
    /**
     * Last error number and Human-readable string representing last error
     */
    get error(): { num: number, string: string }

    /**
     * Name of container
     * @returns {string} current name of container
     */
    get name(): string;
    /**
     * Rename a container
     * @param newName {string} New name to be used for the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a369207c40b4946a04026861bfef3d86c)
     */
    set name(newName: string);
    /**
     * Determine if /var/lib/lxc/$name/config exists.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a84fd83757b47401b95ff01efae76d896)
     */
    get defined(): boolean;
    /**
     * Determine state of container.
     * @returns `true` if defined `false` if not
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aa071116fd2f39c4cbe62d171566d14b9)
     */
    get state(): ContainerState;
    /**
     * Determine if container is running.
     * @returns `true` if running `false` if not
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab810a7cd940111dbb480eb0440f71abe)
     */
    get running(): boolean;
    /**
     * Determine process ID of the containers' init process.
     * @returns pid of the init process as seen from outside the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a8ce63dc530f57a3a198ed2d0f0c229a9)
     */
    get initPID(): number;
    /**
     * Current config file name.
     * @returns {string} config file name.
     * @link [https://linuxcontainers.org/apidoc]()
     */
    get configFileName(): string;

    /**
     * Whether container wishes to be daemonized
     * @returns {boolean}
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aad6d903f5f22937f0662814ee4c1ac05)
     */
    get daemonize(): boolean;
    /**
     * Whether container wishes to be daemonized
     * @param value {boolean}
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aad6d903f5f22937f0662814ee4c1ac05)
     */
    set daemonize(value: boolean);

    /**
     * Determine the full path to the containers' configuration file.
     * Each container can have a custom configuration path.
     * However, by default it will be set to either the LXCPATH configure variable, or the lxcpath value in the LXC_GLOBAL_CONF configuration file (i.e. /etc/lxc/lxc.conf).
     * @returns full path to configuration file
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a7244de1a9590f9650bf75b23f1fce0ee)
     */
    get configPath(): string;
    /**
     * Set the full path to the containers' configuration file.
     * @param path {string} Full path to configuration file.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ad140523960327ab5537629ee8dc62dd3)
     */
    set configPath(path: string);

    new(name: string, configPath?: string, alt_file?: string): Container;

    /**
     * Freeze running container.
     * @returns {Promise<void>} if there is any error the promise is rejected
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab83d343ed927520ee693c3c0fc3514ca)
     */
    freeze(): Promise<void>;
    /**
     * Thaw a frozen container.
     * @returns {Promise<void>} if there is any error the promise is rejected
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ac1710a6788be63a2429918ec3724641e)
     */
    unfreeze(): Promise<void>;
    /**
     * Load the specified configuration for the container.
     * @param alt_file {string} Full path to alternate configuration file
     * @returns {Promise<void>} if there is any error the promise is rejected
     */
    loadConfig(alt_file: string): Promise<void>;
    /**
     * Start the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a599d630c220ee5ef2a9c6b79c7eb3b23)
     * @param useinit {boolean} Use lxcinit rather than /sbin/init.
     * @param argv {string[]} Array of arguments to pass to init.
     */
    start(useinit?: number, argv?: string[]): Promise<void>;
    /**
     * Stop the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a661d604dd5d29b5d6dd5d141d63c8539)
     * @returns {Promise<void>} if there is any error the promise is rejected
     */
    stop(): Promise<void>;
    /**
     * Change whether the container wishes all file descriptors to be closed on startup.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a2c979929461fcb4ebe96baed2b0acb8f)
     * @param state {boolean} Value for the close_all_fds.
     * @returns {Promise<void>} if there is any error the promise is rejected
     */
    wantCloseAllFds(state: boolean): Promise<void>; //Check if state is used in c bindings
    /**
     * Wait for a container to be in a specified state
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a103d932d1b2ba4e8b07100ef4dbd053f)
     * @param state {ContainerState} State to wait for.
     * @param timeout {number} @default: -1 Timeout in seconds. 0 to avoid waiting
     * @returns {Promise<void>} When resolved the container is the specified state, when rejected the container wait timed-out or an error occurred
     */
    wait(state: ContainerState, timeout?: number): Promise<void>;
    /**
     * Set a key/value configuration option.
     * @param key {string} config key
     * @param value {string} config value
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a340914109541781444138be66b4e88eb)
     * @example
     * c.setConfigItem('lxc.log.file', '/tmp/logs/ct2.log');
     */
    setConfigItem(key: string, value: string): void;
    /**
     * Delete the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a284790c42e39e597eae61ae2d2995173)
     * @param options
     * @returns {Promise<void>} if there is any error the promise is rejected
     * @note **Container must be stopped and have no dependent snapshots.**
     */
    destroy(options?: {
        /**
         * include all snapshots of container
         * @type {boolean | undefined}
         * @default false
         */
        include_snapshots?: boolean,
        /**
         * Forcefully stop the container and then destroy
         * @type {boolean | undefined}
         * @default false
         */
        force?: boolean
    }): Promise<void>
    /**
     * Save configuration to a file.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#abea85657b8f816e7d2d5238c0d731a67)
     * @param alt_file Full path to file to save configuration in.
     * @returns {Promise<void>} if there is any error the promise is rejected
     */
    save(alt_file: string): Promise<void>;
    /**
     * Create a container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ad2a402be12791e43dd016bba24e53b40)
     * @returns {Promise<void>} if there is any error the promise is rejected
     * @param options
     */
    create(options: {
        /**
         * Template to execute to instantiate the root filesystem and adjust the configuration.
         */
        template?: string,
        /**
         *    Arguments to pass to the template
         */
        argv?: string[]
        /**
         *    Backing store type to use
         *    @default dir
         */
        bdevtype?: string,
        /**
         * Additional parameters for the backing store (for example, LVM volume group to use).
         */
        bdev_specs?: Partial<bdev_specs>,
        /**
         * Additional parameters for the backing store (for example, LVM volume group to use).
         */
        flags?: LXC_CREATE
    }): Promise<void>;
    /**
     * Request the container reboot by sending it SIGINT.
     * @param timeout Seconds to wait before returning false.
     * (-1 to wait forever, 0 to avoid waiting).
     * default: -1 (forever)
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a5c6f030b9e8797cf9a4586b1fab6c3eb)
     */
    reboot(timeout?: number): Promise<boolean>; //TODO Implement reboot send
    /**
     * Request the container shutdown by sending it `SIGPWR`.
     * @param timeout Seconds to wait before rejecting (-1 to wait forever, 0 to avoid waiting).
     * default: -1 (forever)
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a9cfba9429c159612a8d6b768f6e27ade)
     */
    shutdown(timeout?: number): Promise<boolean>;
    /**
     * Completely clear the containers in-memory configuration.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a2d40919280dbedd9d29417d5ca117e86)
     */
    clearConfig(): void;
    /**
     * Clear a configuration item.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a4521fb10eee10cf6ca9b33d3ef1f996a)
     * @param key {string} Name of the option to clear.
     * @see analog of {setConfigItem}
     */
    clearConfigItem(key: string): void;
    /**
     * Retrieve the value of a config item.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a7411db2a0e7a8471c41a4a58d8e83b33)
     * @param key {string} Name of the option to get.
     */
    getConfigItem(key: string): string | undefined;
    /**
     * Retrieve the value of a config item from a running container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aeb0f5f1589ba5dc14d5d1e1c0cb87f96)
     * @param key
     */
    getRunningConfigItem(key: string): string;
    /**
     * Retrieve a list of config item keys given a key prefix.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a885bbd0d02c3771bcee7b02a2c398be9)
     * @param prefix
     */
    getKeys(prefix?: string): string[]
    /**
     * Obtain a list of network interfaces.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a50d09300fa69182156b58eb9ef288dbd)
     */
    getInterfaces(): Promise<string[]>
    /**
     * Determine the list of container IPv4 addresses.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a70b47b5719463e4c61ed097b285d9371)
     * @param iface {string} interface name
     * @param family {string} address family inet
     */
    getIPs(iface: string, family: "inet"): Promise<string[]>
    /**
     * Determine the list of container IPv6 addresses.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a70b47b5719463e4c61ed097b285d9371)
     * @param iface {string} interface name
     * @param family {string} address family
     * @param scope {string} IPv6 scope id
     */
    getIPs(iface: string, family: "inet6", scope: number): Promise<string[]>
    /**
     * Retrieve the specified cgroup subsystem value for the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a6202bf97cd77e208eed1845d5a911cbc)
     * @param subsys {string} cgroup subsystem to retrieve.
     */
    getCGroupItem(subsys: string): string | undefined;
    /**
     * Set the specified cgroup subsystem value for the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aff7753133cff04ed1e48fdceaf490a28)
     * @param subsys {string} cgroup subsystem to consider.
     * @param value {string} Value to set for `subsys`.
     */
    setCGroupItem(subsys: string, value: string): void;
    /**
     * Copy a stopped container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ada03abc45ce41a3229837858a25de841)
     * @param options {lxc_clone_options}
     * @returns {Promise<Container>} returns a class instance of the container clone.
     * @note
     * If devtype was not specified, and flags contains LXC_CLONE_SNAPSHOT then use the native bdevtype if possible, else use an overlayfs.
     */
    clone(options: lxc_clone_options): Promise<Container>;
    /**
     * Allocate a console tty for the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a64b4ce75b807b21e42d4ccd024652973)
     * @param ttynum
     * @returns {Promise<[number, number]>} [ttyfd, ptxfd]
     * @note
     * On successful return, ttynum will contain the tty number that was allocated.
     * The returned file descriptor is used to keep the tty allocated.
     * The caller should call close(ttyfd and ptxfd) on the returned file descriptor when no longer required so that it may be allocated by another caller.
     */
    consoleGetFds(ttynum?: number): Promise<[number, number]>

    consoleAsync(ttynum: number): Promise<{
        // @ts-ignore
        on(event: 'data', listener: (chunk: Buffer) => void): this;
        write(data: string | Buffer): void;
        close(): void;
    }>
    /**
     * Allocate and run a console tty.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a5ef781838651c315c8cdc6730586a554)
     * @param ttynum {number} Terminal number to attempt to allocate, -1 to allocate the first available tty or 0 to allocate the console.
     * @param stdio {[number,number,number]} [stdinfd, stdoutfd, stderrfd]
     * @param escape {number} The escape character (1 == 'a', 2 == 'b', ...).
     * @returns {Promise<void>} The promise will not return until the user has exited the console.
     */
    console(ttynum: number, stdio: [number, number, number], escape: number): Promise<void>;
    /**
     * Create a shell attached to a container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a1fd6ce7695e6a967efbb9dcc36952168)
     * @param options {lxc_attach_options}
     * @see {DEFAULT_ATTACH}
     * @returns {number} exit status of the process inside the container
     */
    attach(options?: Partial<lxc_attach_options>): Promise<number>;
    /**
     * Create a container snapshot.
     * Assuming default paths, snapshots will be created as /var/lib/lxc/<c>/snaps/snap<n> where <c> represents the container name and <n> represents the zero-based snapshot number.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ab81bbbabd39179066c4bca619e3bcb7d)
     * @param commentfile {string} Full path to file containing a description of the snapshot.
     */
    snapshot(commentfile: string): Promise<number>;
    /**
     * Obtain a list of container snapshots.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aba8a7bc21d7a805110563f5446d9fe82)
     * @returns {lxc_snapshot[]} array of snapshots
     */
    snapshotList(): Promise<lxc_snapshot[]>
    /**
     * Create a new container based on a snapshot.
     * The restored container will be a copy (not snapshot) of the snapshot, and restored in the lxcpath of the original container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#acc0a13de063830a2ccee54ff88f57f79)
     * @param snapname {string} Name of snapshot.
     * @param newname {string} Name to be used for the restored snapshot.
     *
     * @warning If newname is the same as the current container name, the container will be destroyed. However, this will fail if the snapshot is overlay-based, since the snapshots will pin the original container.
     * @note As an example, if the container exists as `/var/lib/lxc/c1`, snapname might be 'snap0' (representing `/var/lib/lxc/c1/snaps/snap0`). If newname is c2, then snap0 will be copied to `/var/lib/lxc/c2`.
     */
    snapshotRestore(snapname: string, newname?: string): Promise<void>
    /**
     * Destroy the specified snapshot.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a80009a1f8c81e8c0776e9e05d18f0d6b)
     * @param snapname {string} Name of snapshot.
     */
    snapshotDestroy(snapname: string): Promise<void>
    /**
     * Destroy all the container's snapshot.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a16c2f4e7f308f8b1c0b281145cc9c845)
     * @param all {boolean}
     */
    snapshotDestroy(all: true): Promise<void>
    /**
     * Add a specified device to the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a269156192f005e5f3cd44f5834637023)
     * @param src_path {string} Full path of the device.
     * @param dest_path {string} Alternate path in the container (or `undefined` to use src_path).
     */
    addDeviceNode(src_path: string, dest_path?: string): Promise<void>
    /**
     * Remove a specified device from the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a3866a5cc79af7e8d8d6b15055f356f73)
     * @param src_path {string} Full path of the device.
     * @param dest_path {string} Alternate path in the container (or `undefined` to use src_path).
     */
    removeDeviceNode(src_path: string, dest_path?: string): Promise<void>
    /**
     * Add specified netdev to the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a33aadfb28b145095bb7213b6bfb02b57)
     * @param dev {string}  name of the net-device.
     * @param dst_dev {string | undefined} name inside container of net-device.
     * use `undefined` to use the same name as host
     */
    attachInterface(dev: string, dst_dev?: string): Promise<void>
    /**
     * Remove specified netdev from the container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ac00d4ee3286ca5c0f2d4283c283fc0ec)
     * @param dev {string} name of the net-device.
     * @param dst_dev {string | undefined}  name inside container of net-device.
     * use `undefined` to use the same name as host
     */
    detachInterface(dev: string, dst_dev?: string): Promise<void>
    /**
     * Checkpoint a container.
     * @link [https://linuxcontainers.org/apidoc](/https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a0a034333ca81e2a80b5e52bfadd73c5b)
     * @param directory {string} The directory to dump the container to.
     * @param stop {boolean | undefined} Whether or not to stop the container after checkpointing.
     * @default false (don't stop container after taking checkpoint)
     * @param verbose {boolean | undefined} Enable criu's verbose logs.
     * @default false
     */
    checkpoint(directory: string, stop?: boolean, verbose?: boolean): Promise<void>
    /**
     * Restore a container from a checkpoint.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#ac7c05a5b4bd5e85557dc080251da2db1)
     * @param directory {string} The directory to dump the container to.
     * @param verbose {boolean | undefined} Enable criu's verbose logs.
     * @default false
     */
    restore(directory: string, verbose?: boolean): Promise<void>
    /**
     * An API call to perform various migration operations.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aaa8cbcb64d51efb940789863d22e9572)
     * @param cmd {LXC_MIGRATE} One of the MIGRATE_ constants.
     * @param options {migrate_opts | undefined} A migrate_opts object filled with relevant options.
     * //TODO: CHECK IF OPTIONS IS ALLOWED TO BE UNDEFINED
     */
    migrate(cmd: LXC_MIGRATE, options?: Partial<migrate_opts>): Promise<void>
    /**
     * Query the console log of a container.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a1d9a42547c16a3350e47e9b8978692c1)
     * @param options {lxc_console_log} A lxc_console_log object filled with relevant options.
     * @returns {Promise<string>} string with the logs.
     */
    consoleLog(options?: Partial<lxc_console_log>): Promise<string>
    /**
     * Mount the host's path source onto the container's path target.
     * @link [https://github.com/lxc/lxc](https://github.com/lxc/lxc/blob/main/src/lxc/lxccontainer.h#L841)
     * @param source
     * @param target
     * @param filesystemtype
     * @param mountflags
     * @param mnt
     */
    mount(source: string, target: string, filesystemtype: string | undefined, mountflags: bigint | number | LXC_MOUNT, mnt: lxc_mount): Promise<void>
    /**
     * Unmount the container's path target.
     * @link [https://linuxcontainers.org/apidoc](//TODO)
     * @param source
     * @param mountflags
     * @param mnt
     */
    umount(source: string, mountflags: bigint | number, mnt: lxc_mount): Promise<void>
    /**
     * Retrieve a file descriptor for the container's seccomp filter.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#aa94c6d2e4f992a9323988e16c05215c2)
     * @returns {Promise<number>} file descriptor for container's seccomp filter
     */
    seccompNotifyFd(): Promise<number>
    /**
     * Retrieve a file descriptor for the running container's seccomp filter.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a0247dee4857aa62cff8e76d007e33848)
     * @returns {Promise<number>} file descriptor for the running container's seccomp filter
     */
    seccompNotifyFdActive(): Promise<number>
    /**
     * Retrieve a pidfd for the container's init process.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a8ce63dc530f57a3a198ed2d0f0c229a9)
     * @returns {Promise<number>} pidfd of the init process of the container.
     */
    initPIDFd(): Promise<number>
    /**
     * Retrieve a mount fd for the container's devpts instance.
     * @link [https://linuxcontainers.org/apidoc](https://linuxcontainers.org/lxc/apidoc/structlxc__container.html#a863a304c9c98e570b016d59ec2016725)
     * @returns {Promise<number>} Mount fd of the container's devpts instance.
     */
    devptsFd(): Promise<number>
    /**
     * Run a command in the container.
     * @param options {Partial<lxc_attach_options> & {argv: string[]}} argv[0] => program, argv[1-n] = program args
     * @returns {Promise<number>} PID of the process running in the container
     */
    exec(options: Partial<lxc_attach_options> & { argv: string[] }): Promise<number>;
}


//endregion

export type LXC = {
    /**
     * @link [https://linuxcontainers.org/apidoc](//TODO)
     * @constructor
     */
    GetVersion(): string,
    /**
     * @link [https://linuxcontainers.org/apidoc](//TODO)
     * @param key
     * @constructor
     */
    GetGlobalConfigItem(key: string): string
    /**
     * @link [https://linuxcontainers.org/apidoc](//TODO)
     * @param lxcpath
     * @constructor
     */
    ListAllContainers(lxcpath?: string): string[],
    /**
     * @link [https://linuxcontainers.org/apidoc](//TODO)
     * @param lxcpath
     * @constructor
     */
    ListAllDefinedContainers(lxcpath?: string): string[],
    /**
     * @link [https://linuxcontainers.org/apidoc](//TODO)
     * @param lxcpath
     * @constructor
     */
    ListAllActiveContainers(lxcpath?: string): string[],
    /**
     * @link [https://linuxcontainers.org/apidoc](//TODO)
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
