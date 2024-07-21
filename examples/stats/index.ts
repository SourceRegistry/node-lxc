import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (!c.defined) {
        throw "Container not defined"
    }
    const memUsed = c.getCGroupItem("memory.usage_in_bytes");
    console.log("MemoryUsage:", memUsed);

    const memLimit = c.getConfigItem("memory.limit_in_bytes");
    console.log("MemoryLimit:", memLimit);

    const kmemUsed = c.getConfigItem("memory.kmem.usage_in_bytes");
    console.log("KernelMemoryUsage:", kmemUsed);

    const kmemLimit = c.getConfigItem("memory.kmem.limit_in_bytes");
    console.log("KernelMemoryLimit:", kmemLimit);

    const swapUsed = c.getConfigItem("memory.memsw.usage_in_bytes");
    console.log("MemorySwapUsage:", swapUsed);

    const swapLimit = c.getConfigItem("memory.memsw.limit_in_bytes");
    console.log("MemorySwapLimit:", swapUsed);

    const blkioUsage = c.getConfigItem("blkio.throttle.io_service_bytes"); //TODO: Go bindings more complex on nested items and io services
    console.log("BlkioUsage:", blkioUsage);

    const cpuTime = c.getConfigItem("cpuacct.usage");
    console.log("cpuacct.usage:", cpuTime);

    const cpuTimePerCPU = c.getConfigItem("cpuacct.usage_percpu");  //TODO: Go bindings more complex on nested cpuTimes
    console.log("cpuacct.usage_percpu:", cpuTimePerCPU);


    const cpuStats = c.getConfigItem("cpuacct.usage_percpu");  //TODO: CPUStats returns the number of CPU cycles (in the units defined by USER_HZ on the system)
                                                                   //TODO: consumed by tasks in this cgroup and its children in both user mode and system (kernel) mode.
    console.log("cpuacct.stat:", cpuStats);


    const interfaceStats = c.getConfigItem("cpuacct.usage"); //TODO: InterfaceStats returns the stats about container's network interfaces
                                                                 // @see https://github.com/lxc/go-lxc/blob/v2/container.go#L1707
    console.log("InterfaceStats:", interfaceStats);


}

main().catch(console.error)