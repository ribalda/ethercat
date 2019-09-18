Etherlab master patchset 20190904
=================================

This is an unofficial patchset maintained by Gavin Lambert <gavin.lambert@tomra.com>.

Patches in the set were contributed by various authors; examine the patches themselves for more information.

The patchset is currently based on revision [33b922](https://sourceforge.net/p/etherlabmaster/code/ci/33b922ec1871/tree/) (default branch) of the [IgH EtherCAT Master library](http://etherlab.org/en/ethercat/index.php), also including additional commits from the stable-1.5 branch up to revision [0c011d](https://sourceforge.net/p/etherlabmaster/code/ci/0c011dc6dbc4facb3ee75f100181ce89814ecefa/tree/).

The patchset has been tested and should be safe for use.  However neither the patches nor the patchset is *stable*, in the sense that future versions of the patchset may add, remove, reorder, or modify existing patches; although other than as needed to resolve bugs this should normally only occur when rebasing the patches against upstream changes.

In some cases you might want to use only a subset of patches. The code should compile and run if you stop at any point (apply all patches prior to a given patch and none afterwards).  Other combinations of patches may produce fuzz or fail to apply or fail to compile or work, depending on the dependencies and overlap of the individual patches selected.  Patches have been organised into directories both to reduce clutter and to provide additional context on which patches most directly belong together.  (Patches under `features` are more optional than other patches, but may still result in fuzz or failed application of other patches if omitted or applied in a different order than specified in the `series` file.)

Feedback on the patchset should be posted to the [etherlab-dev mailing list](http://lists.etherlab.org/mailman/listinfo/etherlab-dev).  Remember to mention which version of the patchset you are using.

Realtime API compatibility
--------------------------

Some of the patches change the ABI between kernel and user applications. When using ecrt.h APIs, it is important to verify the interface version *first thing* in your application, like so:

    if (ecrt_version_magic() != ECRT_VERSION_MAGIC) {
        fprintf(stderr, "Expecting EtherCAT API version %x but found %x.\n",
                ECRT_VERSION_MAGIC, ecrt_version_magic());
        exit(1);
    }

If this error occurs in your application, it means you need to recompile either the Etherlab modules or your application or both.

The userspace library already does this to verify compatibility between the kernel and userspace library itself; but you should still also do so in your userspace application to verify compatibility between the userspace library and your application, or in your kernelspace application to verify compatibility between the EtherLab module and your own module.

This relies on the `ECRT_VERSION_MAGIC` value being updated whenever changes to the structures in ecrt.h have been made.  As several patches in this set make such modifications but you might not apply them all at once, this is mostly left to your own discretion and not included in the individual patches (as this might cause conflicts if applied out of original order); however the patchset does contain a separate patch that updates the value to be different from the upstream sources, just in case you forget.

Ioctl compatibility
-------------------

Similarly, when using the `ioctl` interface, you should verify the interface version for this as well, using code such as this:

    ec_ioctl_module_t mod;
    if (ioctl(fd, EC_IOCTL_MODULE, &mod) < 0) {
        perror("EC_IOCTL_MODULE");
        exit(1);
    }
    if (mod.ioctl_version_magic != EC_IOCTL_VERSION_MAGIC) {
        fprintf(stderr, "Expecting EtherCAT ioctl version %u but found %u.\n",
                EC_IOCTL_VERSION_MAGIC, mod.ioctl_version_magic);
        exit(1);
    }

Where `fd` is an already-open descriptor to any EtherCAT master device.  Again, if this occurs it means you need to recompile things.

This relies on the `EC_IOCTL_VERSION_MAGIC` value being updated whenever a change to the IOCTLs or the associated structures is made.  Again, some of the patches make such changes, but don't modify the version in their own patch; instead there is a separate patch that updates it once.

Using the patches
-----------------

1. Install Mercurial, if you don't already have it, eg:

        sudo apt install mercurial

2. Create or edit your `~/.hgrc` file and ensure it has at least the following contents:

        [extensions]
        mq =

3. Clone the main upstream repository:

        hg clone -u 33b922ec1871 http://hg.code.sf.net/p/etherlabmaster/code etherlab

4. Clone the patch repository:

        hg clone http://hg.code.sf.net/u/uecasm/etherlab-patches etherlab/.hg/patches

5. Apply the patches:

        cd etherlab
        hg qpush -a

6. You're now ready to build, as usual:

        ./bootstrap
        ./configure --help

    And then choose the options you wish, and follow the rest of the instructions in the `INSTALL` file.

Updating from an older version of the patches
---------------------------------------------

The recommended method is to delete your old clones and make new ones, following the instructions from the new patchset.  This is the simplest method to get everything identical to the expected configuration.

Alternatively (to save a bit of network traffic), you can try updating in-place as follows:

    cd etherlab
    hg qpop -a
    hg pull -u --mq

At this point you will have the new patches; read the new `.hg/patches/README.md` to see if it's based on the same upstream or not; do the following to update the upstream if necessary:

    hg pull
    hg update 33b922ec1871

(Remember to use the hash from the new README.)  And finally apply the new patches with:

    hg qpush -a
    autoreconf

Omitting some patches
---------------------

Before issuing the `hg qpush -a` command, you can edit the `series` file and comment out (with a leading `#`) or remove some lines to exclude those patches.  You can optionally use special "guards" to only conditionally apply certain patches, although these might not be compatible with other patch management systems.

Note that the patchset is structured assuming that all patches are applied in the order specified in the series file; if some patches are skipped or applied out of order then you might get fuzz or failed hunks, and some code might fail to compile.

Using with another version control system
-----------------------------------------

After you have updated an HG working copy as above, you might want to export the result into another version control system for your main application.  There are many ways to do this, but the simplest is just to copy the resulting source (excluding the .hg directory) into your other source tree and commit it as a single delta from the prior version, reapplying any further custom patches you have on top.  Some VCSes may have other recommended practices, such as vendor branches or rebasing your custom patches as individual commits on top of the new source.

While we recommend applying the patchset via Mercurial as shown above, it's also possible to apply them via another method, such as using quilt to apply patches on top of a source tarball, or importing that same tarball into Git and then importing each patch as an individual git commit.  In either case, for best results you should follow the application order specified in the series file.

For example, assuming that you have an HG working copy with all desired patches applied as above, you can export this into a Git-compatible patch file (which can be imported as a series of commits with `git am`) by issuing the command `hg export --git -r NNNN: > all.patch`, where NNNN is the id of the first patch (which you can get from `hg log`; an example can't be provided as it will change as commits are added to the main repository, even on different branches).

If for some reason you *really* don't want to apply patches in a Mercurial working copy first, you can run the included `mkmbox.sh` script to generate a roughly equivalent mbox file, which can then be imported with `git am --whitespace=nowarn -m all.mbx` (the `-m` is optional but includes the original patch name in the commit message for convenience).  You must have previously imported and committed the pristine upstream source for the HG changeset specified above.  Note that this script is not compatible with guards, although it can handle lines that have been deleted or commented out.

E1000E warning
--------------

If you are using a motherboard-based e1000e adapter with the `ec_e1000e` driver on Linux 4.4 or above, a recent change in the `mei_me` hardware driver is known to cause problems with some hardware and kernel configurations.

Add-in e1000e cards do not appear to be affected, nor is the `ec_generic` driver (even when using an otherwise affected adapter).

The observed behaviour is that while on initial boot everything communicates as expected, if you disconnect and then reconnect the EtherCAT network cable from the master (or reboot the first slave), all EtherCAT datagrams will time out; even after link is established.  Replugging the cable makes no difference, but restarting the etherlab service will temporarily recover until next link-down.

To work around this for affected systems, create `/etc/udev/rules.d/20-mei.rules` with the following content:

    ACTION=="add",KERNEL=="mei0",ATTR{../../power/control}="on"

It is completely harmless to include this on systems that do not contain Intel MEI hardware.  On systems that do, this will disable power suspend for this device, which is unlikely to cause any problems for systems running EtherCAT.

Patch directories
-----------------

Patches are intended to be applied from directories in the following order:

* stable
    This directory contains commits made to the `stable-1.5` branch that have not yet been merged to `default` in the main repository.  (As such, these are slightly more "official" than the rest of the patches.)

* linux
    This directory contains patches to fix compilation errors under newer versions of the Linux kernel.  It does not include new device drivers, however.

* devices
    This directory contains patches to Ethernet device drivers or support for additional kernel versions.  Note that for the most part new driver versions are only guaranteed to compile (and for the orig source to match that in the corresponding kernel); while we have made our best effort to port forward the associated EtherCAT modifications to the drivers, they have not been tested and it is possible that there may be errors or omissions and the driver might not work correctly or at all.

* base
    This directory contains all the bug-fix patches and other important things that should generally always be used.

* features
    This directory contains new features or similar improvements that are generally optional.

Patch list
----------

##### stable/*

These are not documented individually, but as noted above consist of several commits made on the stable-1.5 branch that have not yet been merged to default.

Currently, some significant changes included here are updates to the ccat driver and the addition of the igb driver.

##### linux/0001-debugif-3.17.patch

Fix alloc_netdev call in debug-if for Linux 3.17 and later.

##### linux/0002-kern_cont.patch

Adds `KERN_CONT` markers to `printk` calls intended to continue the previous line.

While this has been recommended practice for a while, somewhere around kernel 4.9 it started printing erroneous newlines if you didn't do it.  This caused the `ethercat debug 1` output in particular to get quite messed up.

##### linux/0003-vm_fault-4.10.patch

Fixes a compile error from kernel 4.10 and above.

**NOTE**: upstream (in patch stable/0007) the VM address was removed from this same code, which also resolves the related error.  This patch now restores it; it's unknown whether it was removed because it was unnecessary or uninteresting or just because that was easier than fixing it as in this patch.

##### linux/0004-signal-4.11.patch

Fix build on kernel 4.11.

##### linux/0005-tty-4.15.patch

Timer API changes and other fixes to TTY module for kernel 4.15 and later.

##### devices/0001-update.patch

Fixes some inconsistencies in the upstream device update scripts (after stable patches).

##### devices/0002-update.patch

A minor tweak to the `update.sh` device helper scripts to make rejects easier to understand.

##### devices/0003-e1000e-link-detection.patch

Fixes link detection in e1000e driver for 3.10-3.16.

##### devices/0004-linux-3.18.patch

Updates 8139too, r8169, e100, e1000, and e1000e drivers to kernel 3.18.  (The igb driver was originally supplied for this kernel.)

##### devices/0005-linux-4.1.patch

Updates 8139too, r8169, e100, e1000, e1000e, and igb drivers to kernel 4.1.

##### devices/0006-linux-4.4.patch

Fixes some bugs in the stable version of the igb driver for kernel 4.4.

##### devices/0007-linux-4.9.patch

Updates 8139too, r8169, e100, e1000, e1000e, and igb drivers to kernel 4.9.

##### devices/0008-linux-4.14.patch

Updates 8139too, r8169, e100, e1000, e1000e, and igb drivers to kernel 4.14.

##### devices/0009-linux-4.19.patch

Updates 8139too, e100, e1000, e1000e, and igb drivers to kernel 4.19.
r8169 is not included.

##### devices/0010-cx2100-2.6.patch

Adds device driver for CX2100 for Linux 2.6.32 (only).

##### devices/0011-cx2100-4.9.patch

Adds (untested) device driver for CX2100 for Linux 4.9 (only).

##### devices/0012-e1000-unused-variable.patch

Fixes an uninitialized variable warning in the e1000 driver.

##### devices/0013-r8152-4.9.patch

Support for Realtek RTL8152/RTL8153 for Linux 4.9.

##### devices/0014-r8152-3.18.patch

Support for Realtek RTL8152/RTL8153 for Linux 3.18.

##### devices/0015-r8152-4.4.patch

Support for Realtek RTL8152/RTL8153 for Linux 4.4.

##### base/0000-version-magic.patch

Increment interface versions.

This is a placeholder assuming that you will be applying patches that change the ABI for both ecrt and ioctl interfaces.

It should not be applied upstream as-is (different numbers should be used), and if you are only applying subsets of patches at a time (and you want to know when you have mismatched library and app code and need to recompile) then you may want to increment the versions as you apply different sets, so your application will give you an error if you forget to recompile something.

##### base/0001-Distributed-Clock-fixes-and-helpers.patch

Adds `ecrt_master_setup_domain_memory` and `ecrt_master_deactivate_slaves`; fixes up application-selected reference clocks.

##### base/0002-Distributed-Clock-fixes-from-Jun-Yuan.patch

This sorts out some timing issues to do with slave dc syncing.

##### base/0003-print-sync-signed.patch

Corrects the DC sync log message to use signed format, since it can be given negative values.

##### base/0004-fix-eoe-clear.patch

Fixes typo in EoE request cancellation on slave disposal.

##### base/0005-disable-eoe.patch

Fixes compilation with `--disable-eoe`.

##### base/0006-avoid-ssize_t.patch

Fixes an unneeded use of `ssize_t` which caused a compile error in one environment.

##### base/0007-replace-fprintf-calls-with-EC_PRINT_ERR.patch

Avoids `fprintf(stderr)` when using userspace RTAI via RTDM.

##### base/0008-read-reference-slave-clock-64bit-time.patch

Added functions to queue and read the reference slave clock's 64bit time.  Mainly for diagnostics.

##### base/0009-Avoid-changing-running-slaves-DC-offset.patch

If the network rescans while the master app is running (eg. due to a change in the number of responding slaves), then without any patch the reconfig process is likely to update the System Time Offset register, which causes an immediate step change in the slave’s DC clock and can in turn sometimes result in it missing pulses and possibly stopping altogether.)

* If the System Time Offset and Delay registers for a given slave are already correct, it will not write to them at all.
* If it wants to change the System Time Offset register but the slave is already in SAFEOP or OP, then it won’t change it (but will still update the System Time Delay with the transmission delay).
    * Modifying the System Time Offset register (0x0920) causes a step change in the System Time of the slave, which can cause it to miss the next sync time (for a 32-bit slave, it can take 4 seconds to recover (likely out of phase), and for a 64-bit slave, it might never sync again).
    * Modifying the System Time Delay register (0x0928) just alters the value it uses when the normal time sync datagram circulates (as far as I can tell); this is drift compensated so it will gradually drift to the correct time instead of stepping straight to it, so shouldn't cause the above problem.
    * Patches base/0001 and base/0002 both make it more likely for the master to want to update the System Time Offset (though they do improve other things, and this is good for the initial startup case – just less so for the reconfigure-during-OP case).
* If it updated the offset (which only happens when the slave is not in SAFEOP or OP) it will also write register 0x0930 to reset the drift filter (this is recommended in the datasheets).

This should now be cleaner and safer than the previous version of this patch (which disabled and re-enabled sync outputs, which only works if the slave supports and enables AssignActivate 0x2000 and might miss some pulses), and better for general use, since this allows running slaves to always use drift compensation to adjust their clocks gradually rather than stepping instantly (while slaves being configured from PREOP can still step immediately).

The patch now applies to all DC-capable slaves – previously it only affected slaves that use sync pulse generation (AssignActivate 0x0100), but step changes can be a problem for slaves that perform DC timestamping or other things too.

It should also avoid DC timing shifts during operation, especially if you are using a slave as the reference clock and syncing the master clock to it rather than the reverse.  (Note that the `dc_user` example code does do the reverse and actually uses the master clock as the real reference.  If you’re not sure which is which, using `ecrt_master_reference_clock_time` to get the slave refclock time and use it in the master code is the former, while using `ecrt_master_sync_reference_clock` to send the master clock to the slave refclock is the latter.  Both approaches should be safe but the latter is subject to higher jitter and drift.)

However bear in mind that the way I’m using DC I’m not going to notice small timing errors.  So I’d appreciate it if someone who is using DC more extensively (especially with motor slaves, which tend to be picky) could verify it.

##### base/0010-Make-busy-logging-a-little-less-irritating.patch

This makes changeset [a2701a] "Internal SDO requests now synchronized with external requests." a bit less noisy.
[a2701a]:https://sourceforge.net/p/etherlabmaster/code/ci/a2701a/tree/

##### base/0011-Reduced-printing-to-avoid-syslog-spam.patch

Removed unecessary printf in lib/master.c and printk in ioctl.c to avoid syslog spam. If ECAT_GetSlaveInfo in system SW is called on a slave that does not exist you get a printf to stderr and printk in syslog. This is however done every 10ms on each IO card in some applications. The already returned error code is enough for proper error handling.

##### base/0012-Added-newline-to-syslog-message-MAC-address-derived.patch

Added newline to syslog message "MAC address derived ..."

##### base/0013-Do-not-reuse-the-index-of-a-pending-datagram.patch

Do not reuse the index of a pending datagram, to prevent corruption.

##### base/0014-Fix-NOHZ-local_softirq_pending-08-warning.patch

Fix NOHZ kernel warning in debug interface.

##### base/0015-Clear-configuration-on-deactivate-even-if-not-activa.patch

Clears the master configuration when `ecrt_master_deactivate` is called even if `ecrt_master_activate` has not been called prior (though it still logs a warning).  In particular, this allows creating slave configs and request objects and then discarding them, eg. to use the async APIs prior to starting the realtime loop.

Note that this does not apply to `ecrt_master_deactivate_slaves` (added by patch base/0001) even though otherwise they have much in common; there is no technical reason for this, it's just not an expected usage pattern.

##### base/0016-If-enable-rtmutex-use-rtmutexes-instead-of-semaphore.patch

Configure option `--enable-rtmutex` makes the master use rtmutexes instead of regular semaphores.  This provides priority inheritance to reduce latency on lock contention.

Former patch 0007 has been folded into this and then modified to correct a deadlock issue, especially with EoE and/or RTAI.

##### base/0017-Master-locks-to-avoid-corrupted-datagram-queue.patch

Adds locks to protect functions that are likely to be called from multiple concurrent application tasks.  In some cases applications may still require external locks.

Linux locks are skipped when compiled for RTDM to avoid secondary context and deadlocks when called from RT tasks.  If you have multiple RT tasks then it is your responsibility to use the appropriate RTAI/Xenomai locks.

##### base/0018-Use-call-back-functions.patch

Ensures that we use the call back functions (with io lock support) when using the IOCTL interface.

I am not certain whether this patch is RTAI safe or not.  Feedback is appreciated.

##### base/0019-Support-for-multiple-mailbox-protocols.patch

When reading a slave mailbox it may return a response for any prior request in any order, or an unsolicited response (eg. EoE, CoE emergency).  This patch routes the data according to protocol type so that the FSMs do not get confused as long as only one request for any given protocol is in flight for a given slave at a time.

##### base/0020-eoe-ip.patch

Fixes EoE mailbox conflict between IP state machine (added in stable patches) and packet state machine.

Also corrects the format of the IP address packet.

##### base/0021-Await-SDO-dictionary-to-be-fetched.patch

Requests are not processed until the SDO dictionary finishes fetching, to avoid timeouts.

##### base/0022-Clear-slave-mailboxes-after-a-re-scan.patch

When rescanning a slave (and thus discarding any prior pending FSMs), explicitly clear the slave mailbox, to avoid getting confused by a stale response.

##### base/0023-Skip-output-statistics-during-re-scan.patch

UNMATCHED datagrams are expected during a rescan; don't report them.

##### base/0024-Sdo-directory-now-only-fetched-on-request.patch

Only fetch the SDO dictionary when explicitly requested by the tool.  This improves scan performance since it can be quite time consuming, and after initial commissioning it is usually not required.

##### base/0025-Ignore-mailbox-settings-if-corrupted-sii-file.patch

Ignore SII specified mailbox settings if they appear to be from blank EEPROM.

##### base/0026-EoE-processing-is-now-only-allowed-in-state-PREOP.patch

EoE is disabled in INIT, BOOT, and invalid states to prevent errors.

##### base/0027-Prevent-abandoning-the-mailbox-state-machines-early-.patch

Fixes the exit condition of the mailbox FSMs -- previously they returned whether they were sending a datagram or not, and the parent assumed that this meant they were done if they didn't want to send a datagram.  Since earlier patches can cause idle cycles now, this could cause unexpected early exit, so they now return whether they're complete or not explicitly.

##### base/0028-ec_master_exec_slave_fsms-external-datagram-fix.patch

Don’t consume a slave ring datagram when not actually using it.

##### base/0029-Tool-Withdraw-EEPROM-control-for-SII-read-write.patch

The PDI EEPROM control is now withdrawn for all normal SII read and write operations unless EEPROM has been locked by PDI. An EEPROM locked by the PDI can however still be withdrawn using the force option on the SII read and write operations.

##### base/0030-Print-redundancy-device-name-with-ring-positions-as-.patch

Changes several places where a slave's ring position is logged to also display the link name ("main" or "backup").  This is because when master-port redundancy was active, the first slave responding on each link would both have been "0-0" previously; now one is "0-main-0" and the other is "0-backup-0".  This will still affect the log output even if redundancy is not enabled.

##### base/0031-ext-timeout.patch

When an ext_ring (master/slave FSM, rather than application domain) datagram does not fit in the current cycle, it is checked for timeout based on how long it has been since the datagram was populated -- except that this origin time was never actually set, resulting in checking the time since that datagram *slot* was last used to send some **other** datagram.  This resulted in all such datagrams being timed out instead of being deferred to the next cycle as intended.

##### base/0032-dc-sync1-offset.patch

Use the combination of the `sync1_cycle` and `sync1_shift` parameters to `ecrt_slave_config_dc` when calculating the SYNC1 register value.  (For backwards compatibility, you can continue to specify both offsets combined in `sync1_cycle` and leave `sync1_shift` at 0.)

Also resolves an erroneous calculation of the DC start time introduced in b101637f503c for slaves that only want to shift SYNC1 without using a subordinated cycle, or that want to combine a subordinated cycle with a shift.

##### features/xenomai3/0001-Support-Xenomai-version-3.patch

Support for Alchemy RTDM in Xenomai v3.

##### features/net-up/0001-Add-support-for-bringup-up-network-interface-when-st.patch

Add support for bringing up network interface when starting EtherCAT (eg. when using the `generic` driver).

##### features/sii-cache/0001-Improved-EtherCAT-rescan-performance.patch

If a slave has an alias or serial number (and can thus be uniquely identified on the network even if its position changes), its SII can be cached to avoid re-reading it.  This improves scan performance, particularly on larger networks.

##### features/sii-cache/0002-Redundancy-name.patch

Since the prior patch introduces another place where a slave address is printed, this extends base/0028 to it.

##### features/sii-cache/0003-rescan-check-revision.patch

1. The SII cache-and-reuse behaviour can be disabled via `--disable-sii-cache` at configure time, rather than requiring modifying a header file.
2. The revision number is also verified before using the cached version (this resolves some issues when the device firmware is upgraded).
3. If both the alias and serial are read as 0, it will no longer bother reading the vendor/product/revision, as it is now known that the SII is not in the cache.
4. Several similar states are consolidated into one.

##### features/reboot/0001-Add-command-to-request-hardware-reboot-for-slaves-th.patch

Adds `reboot` command to tool, to perform software reboot of slaves that support this (via register 0x0040).

##### features/quick-op/0001-After-a-comms-interruption-allow-SAFEOP-OP-directly-.patch

Allows a slave in SAFEOP+ERR with Sync Watchdog Timeout state (0x001B) to transition straight back to OP (do not pass through PREOP, do not reconfigure).  This lets a device recover faster from a comms interruption that doesn't cause a network structure change.

As a side benefit it also caches the last AL Status code from the slave, which could potentially be made available to the application (although this patch does not do so).

This is enabled by default but you can use `--disable-quick-op` at configure time to disable it if it causes problems with certain DC slaves.

##### features/status/0001-Adding-some-more-state-to-avoid-calling-the-more-exp.patch

Adds some extra fields to the ecrt.h interface so the application can know a bit more about what's going on and whether requests are likely to take a while to start.

##### features/status/0002-Detect-bypassed-ports-timestamp-not-updated.patch

When doing delay measurement, detects ports that are completely bypassed (eg. by a slave-based redundant ring topology), allowing them to be ignored rather than using invalid timestamps.

##### features/status/0003-Calculate-most-likely-upstream-port-for-each-slave.patch

Indicates the most-likely upstream port for each device.  Normally this should always be 0, but it can sometimes be 1-3 if a device has been connected backwards accidentally or as a result of active redundancy.  Useful for diagnostics.

##### features/status/0004-slave-config-position.patch

Adds a `position` field to the structure returned by `ecrt_slave_config_state`.  This allows you to quickly get the ring position of a slave from its relative alias:offset address, which in turn allows you to call other APIs that require this (eg. `ecrt_master_get_slave`).

Note that the `position` is only valid if `online` is true, and that it's possible for the value to be stale (ie. the slave has moved to some other position in the meantime).

##### features/rdwr/0001-Add-register-read-write-support.patch

* Adds `ecrt_reg_request_readwrite` API to perform register read+write requests (writes register data and reads back the prior values in a single datagram).

* Adds `reg_rdwr` command to tool which does the same from the command line.

This can be useful to ensure that the data is coherent and you don't miss an update; particularly useful for read+clear on error counters.

##### features/rdwr/0002-Display-more-info-about-the-register-requests-in-pro.patch

Adds some additional level 1 logging for register requests.

##### features/complete/0001-Support-SDO-upload-via-complete-access.patch

* Adds `ecrt_master_sdo_upload_complete` sync API to read an entire object from a slave via SDO Complete Access (where supported by the slave, although most do).

* Adds complete access functionality to tool `upload` command.  This defaults to the `octet_string` data type so that it can more easily be written to a file or otherwise processed (eg. `ethercat upload -p0 INDEX | hd` will format the data nicely).

As with the existing download functionality only Complete Access from subindex 0 is supported.  Don't forget that subindex 0 is 8-bit value followed by 8-bit padding; don't treat it as a 16-bit value.

##### features/complete/0002-add-sdo-write-with-size.patch

Adds `ecrt_sdo_request_write_with_size` API to allow an SDO request write to be a different size than the immediately prior read.  The requested write size must always be smaller or equal to the size specified when the request was created; while the API will give you an error if this is not the case it means you've already committed a buffer overrun.

Combined with the existing `ecrt_sdo_request_index` API, this allows you to re-use one request object for any SDO on the same slave, provided the initial size is large enough.

Previously you had to use at least one object per different object size, as there was no way to set the write size other than at creation time, and the write size was also altered whenever you did a read.

##### features/complete/0003-sdo-requests-complete.patch

Support for SDO complete access in SDO requests.

Adds `ecrt_slave_config_create_sdo_request_complete`.
Adds `ecrt_sdo_request_index_complete`.

In a previous patchset these were implemented as additional parameters rather than separate methods; while I still think that's the cleaner API (and perhaps the best to integrate upstream), this edition is more compatible with existing code.

##### features/master-redundancy/0001-e1000e-fix-watchdog-redundancy.patch

Changes the e1000e driver such that:

* The watchdog task is executed on a Linux background thread rather than in the RT thread.  (The task can be slow.)
* The watchdog task is executed regardless of whether packets are being received or not.

The latter is required to properly support master-side redundancy (`--with-devices=2`), because in this case one port is only sending packets and the other is only receiving packets.  Without this change, link detection does not work correctly in this case.

This patch may not be safe to use in RTAI.

##### features/sii-file/0001-load-sii-from-file.patch

Adds the ability to override the slave's SII data with a file (useful if the slave's SII EEPROM is too small to hold the correct data).

* The functionality is disabled by default.
* At configure time, you can use `--enable-sii-override` to activate it, using the standard udev/hotplug lookup process.
* At configure time, you can use `--enable-sii-override=/lib/firmware` (or another path) to activate it using a direct loading method.
* It will cooperate as expected with features/sii-cache, although note that it’s not as efficient as it could be (it will reload some of the values that features/sii-cache already read when checking the SII cache; but trying to improve this would make the code really awkward).

##### features/diag/0001-ethercat-diag.patch

Adds a `diag` command to the tool which queries slaves' error counter registers to help locate lost links and other network faults.

**NOTE**: upstream has also added a similar command `crc`; they are not quite identical.  Probably at some point this should be merged with that and the `diag` command would go away.

##### features/diag/0002-diag-readwrite.patch

Modifies the prior patch to use feature `rdwr`'s `reg_rdwr` functionality to improve atomicity of read+reset requests.

##### features/foe/0001-fsm_foe-simplify.patch

* Removes some redundant fields from the FoE FSM; some were unused copy/paste holdovers from the CoE FSM while others were duplicated unnecessarily between the read and write operations, which can’t be concurrent for a given slave anyway.

* Fixes the case where the incoming data exceeds the provided buffer to properly terminate the state machine instead of leaving things dangling.  Although note that this still leaves the FoE conversation itself dangling, so you’ll likely get an error on the next request if this occurs.

##### features/foe/0002-foe-password.patch

* Adds support for sending an FoE password along with read or write requests.

* Implements the `-o` option for the `foe_read` command (which was documented but not implemented).

* Makes the ioctl behind `foe_read` actually use the buffer size requested by the caller (instead of a hard-coded value); though note that `foe_read` itself still uses a hard-coded value of its own (but it’s larger, so bigger files should be readable now).  It’s possible that users on limited-memory embedded systems might need to reduce this value, but it’s still fairly conservative as RAM sizes go.

##### features/foe/0003-foe-requests.patch

Makes FoE transfer requests into public `ecrt_`* API, similar to SDO requests.

Primarily (following my goal of “parallel all the things”), this allows FoE transfers to be non-blocking so that transfers to multiple slaves can occur concurrently from the same requesting thread (previously this was only possible by using separate threads, since the only API was blocking).  Note that due to patch base/0015 you can call `ecrt_master_deactivate()` to delete these requests when you’re done with them, even if you haven’t called `ecrt_master_activate()` yet.

It has a possible side benefit that FoE transfers can now be started and monitored from realtime context, although as FoE is mostly used for firmware updates this is unlikely to be all that useful in practice.

##### features/foe/0004-foe-request-progress.patch

Adds a way to get a “current progress” value (actually the byte offset) for async FoE transfers.

##### features/parallel-slave/0001-fsm_sii_external-datagram.patch

Prep for a later patch: make `fsm_sii` use an external datagram.

##### features/parallel-slave/0002-fsm_change-external-datagram.patch

Prep for a later patch: make `fsm_change` use an external datagram.

##### features/parallel-slave/0003-fsm_slave_config-external-datagram.patch

Prep for a later patch: make `fsm_slave_config` use an external datagram.

##### features/parallel-slave/0004-fsm_slave_scan-external-datagram.patch

Prep for a later patch: make `fsm_slave_scan` use an external datagram.

##### features/parallel-slave/0005-fsm_slave-handles-all-sdos.patch

This moves the internal SDO requests and SDO dictionary requests (if you disable `EC_SKIP_SDO_DICT` from patch base/0023; otherwise dictionary requests already were effectively moved) from `fsm_master` into `fsm_slave`.

This does two important things: firstly it removes the fighting over the CoE mailbox between the internal and external SDO requests (making the busy-checking on each side unnecessary).  And secondly it allows both of these to occur in the background and in parallel between multiple slaves.

##### features/parallel-slave/0006-fsm_slave_config_scan-to-fsm_slave.patch

Similar to the previous patch, this moves `fsm_slave_scan` and `fsm_slave_config` from `fsm_master` to `fsm_slave`.  This allows slave scanning and configuration to occur in parallel for multiple slaves.  (Note that scanning all slaves must complete before configuring any slave can begin.)

This also adds `scan_required` to `ec_slave_info_t`; when true the other fields are unreliable (and should be ignored) as scanning has not yet started or is still in progress.

The motivating case was a network of about 100 slave devices; while scanning is fast (under a second for a "hot" network with aliases/serials on all devices, after prior SII patches), the configuration process to bring the slaves from PREOP to OP took about 80 seconds (and you could see the lights coming on each slave in sequence).  After the patch it takes about 20 seconds.

I actually originally intended to only move `fsm_slave_config`, but the structure of the code required moving `fsm_slave_scan` as well.  Logically they do both belong in the slave FSM anyway.

Note that in this case "parallel" does not mean separate threads – all the FSMs (master and all slaves) still execute on a single thread.  But it can now include datagrams for multiple slaves in the same frame.  The existing throttling mechanism for `fsm_slave` is used, so it will configure slaves in chunks, not all at once (so the network won’t get overloaded if you have a large number of slaves, though network usage will be higher than it previously was).  With default settings it does **16** at a time; this is controlled by `EC_EXT_RING_SIZE/2`.

##### features/parallel-slave/0007-fsm-exec-simplify.patch

Now that most of the FSMs execute from `fsm_slave`, it’s not necessary for them to check the datagram state, as master.c’s `ec_master_exec_slave_fsms` does this in advance.  This simplifies the FSM exec functions.

##### features/sii-wait-load/0001-slave-scan-retry.patch

Retry reading SII if an error occurs.

##### features/sii-wait-load/0002-fsm_sii-loading-check.patch

Retry reading SII if the “loaded” bit is not set.

##### features/rt-slave/0001-allow-app-to-process-slave-requests-from-realtime.patch

Adds `ecrt_master_rt_slave_requests` and `ecrt_master_exec_slave_requests` which allow an application to queue and process background slave requests in the main application cyclic task (or some other RT task).  Potentially useful to enable faster processing of slave requests when using an RTAI application with a slow Linux kernel HZ.

**CAUTION**: there is a higher risk of race conditions with this approach if not used carefully.  Applying the patch is safe even if you do not use this feature, however.

##### features/eoe-rtdm/0001-eoe-addif-delif-tools.patch

Allows EoE interfaces to be configured explicitly in advance and to exist even when their corresponding slaves are absent or not yet configured (this is reflected in the interface's carrier state).

##### features/eoe-rtdm/0002-eoe-via-rtdm.patch

Allows EoE interfaces to be managed by an application thread rather than a master thread; this allows better synchronisation between tasks for userspace RTDM applications.

##### features/pcap/0001-pcap-logging.patch

Adds the `pcap` command, to create a Wireshark-format file containing a fixed amount of EtherCAT packets, for diagnostic purposes.

    ethercat pcap >log.pcap
    ethercat pcap -r >log.pcap

Capturing stops when the internal buffer is full.  Resetting the buffer will restart capturing, but some packets may have been lost in the interim.

The capture log size is hard-coded.

##### features/pcap/0002-runtime-size.patch

Modifies the above to allow the pcap capture buffer size to be specified in the ethercat configuration file rather than at compile time.  Capturing is also disabled by default (to save memory).

This allows you to obtain captures from a production system without reinstalling the master -- merely restarting it.

##### features/pcap/0003-report-size.patch

Fixes an error that could occur if `ethercat pcap` is executed before the capture buffer is full.

##### features/pcap/0004-high-precision.patch

Use microsecond-precision timestamps in pcap files, except when using RTAI/Xenomai.

##### features/mbg/0001-mailbox-gateway.patch

Adds EtherCAT Mailbox Gateway server.  [More Info](http://lists.etherlab.org/pipermail/etherlab-dev/2019/000702.html)
