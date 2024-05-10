/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */
export type bdev_specs = {
    /**
     * Filesystem type
     */
    fstype: string;
    /**
     * Filesystem size in bytes
     */
    fssize: bigint | number;
    zfs: {
        /**
         * ZFS root path
         */
        zfsroot: string
    }; /*  */
    lvm: {
        /**
         * LVM Volume Group name
         */
        vg: string;
        /**
         * LVM Logical Volume name
         */
        lv: string;
        /**
         * LVM thin pool to use, if any
         */
        thinpool: string;
    };
    /**
     * Directory path
     */
    dir: string;
    rbd: {
        /**
         * RBD image name
         */
        rbdname: string;
        /**
         * Ceph pool name
         */
        rbdpool: string;
    }
}
