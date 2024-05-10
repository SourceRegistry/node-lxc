/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

export type migrate_opts = {
    /* new members should be added at the end */
    directory: string
    verbose: boolean

    stop: boolean; /* stop the container after dump? */
    predump_dir: string; /* relative to directory above */
    pageserver_address: string; /* where should memory pages be send? */
    pageserver_port: string;

    /* This flag indicates whether or not the container's rootfs will have
     * the same inodes on checkpoint and restore. In the case of e.g. zfs
     * send or btrfs send, or an LVM snapshot, this will be true, but it
     * won't if e.g. you rsync the filesystems between two machines.
     */
    preserves_inodes: boolean;

    /* Path to an executable script that will be registered as a criu
     * "action script"
     */
    action_script: string;

    /* If CRIU >= 2.4 is detected the option to skip in-flight connections
     * will be enabled by default. The flag 'disable_skip_in_flight' will
     * unconditionally disable this feature. In-flight connections are
     * not fully established TCP connections: SYN, SYN-ACK */
    disable_skip_in_flight: boolean;

    /* This is the maximum file size for deleted files (which CRIU calls
     * "ghost" files) that will be handled. 0 indicates the CRIU default,
     * which at this time is 1MB.
     */
    ghost_limit: bigint | number;

    /* Some features cannot be checked by comparing the CRIU version.
     * Features like dirty page tracking or userfaultfd depend on
     * the architecture/kernel/criu combination. This is a bitmask
     * in which the desired feature checks can be encoded.
     */
    features_to_check: bigint | number;
}