/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

export enum LXC_CLONE {
    /**
     * Do not edit the rootfs to change the hostname
     */
    KEEPNAME = (1 << 0),
    /**
     * Do not change the MAC address on network interfaces
     */
    KEEPMACADDR = (1 << 1),
    /**
     * Snapshot the original filesystem(s)
     */
    SNAPSHOT = (1 << 2),
    /**
     * Use the same bdev type
     */
    KEEPBDEVTYPE = (1 << 3),
    /**
     * Snapshot only if bdev supports it, else copy
     */
    MAYBE_SNAPSHOT = (1 << 4),
    /**
     * Number of `LXC_CLONE_*` flags
     */
    MAXFLAGS = (1 << 5),
    /**
     * allow snapshot creation even if source container is running
     */
    ALLOW_RUNNING = (1 << 6)
}