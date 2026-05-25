/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

import {LXC_CLONE} from "./LXC_CLONE";

export type lxc_clone_options =
    (
        {
            /**
             * New name for the container.
             */
            newname: string
            /**
             * lxcpath in which to create the new container
             * If undefined, the original container's lxcpath will be used.
             * (XXX: should we use the default instead?)
             */
            lxcpath?: string
        } |
        {
            /**
             * New name for the container.
             * If undefined, the same name is used and a new lxcpath MUST be specified.
             */
            newname?: string
            /**
             * lxcpath in which to create the new container
             */
            lxcpath: string
        }
        ) &
    {
        /**
         * Additional LXC_CLONE* flags to change the cloning behaviour:
         */
        flags?: number | LXC_CLONE,
        /**
         * Optionally force the cloned bdevtype to a specified plugin.
         * By default, the original is used (subject to snapshot requirements).
         */
        bdevtype?: string,
        /**
         * Information about how to create the new storage (i.e. fstype and fsdata).
         */
        bdevdata?: string,
        /**
         * In case of a block device backing store, an optional size. If 0, the original backing store's size will be used if possible.
         * @note
         * this only applies to the rootfs.
         * For any other filesystems, the original size will be duplicated.
         */
        newsize?: number,
        /**
         * Additional arguments to pass to the clone hook script.
         */
        hookargs?: string[]
    };