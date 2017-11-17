/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2009-2010  Moehwald GmbH B. Benner
 *                     2011  IgH Andreas Stewering-Bone
 *                     2012  Florian Pose <fp@igh-essen.com>
 *
 *  This file is part of the IgH EtherCAT master.
 *
 *  The IgH EtherCAT master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; version 2 of the License.
 *
 *  The IgH EtherCAT master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT master. If not, see <http://www.gnu.org/licenses/>.
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

/** \file
 * RTDM interface.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <rtdm/driver.h>

#include "master.h"
#include "ioctl.h"
#include "rtdm.h"

/** Set to 1 to enable device operations debugging.
 */
#define DEBUG_RTDM 0

struct ec_rtdm_context {
	struct rtdm_fd *fd;
	ec_ioctl_context_t ioctl_ctx;	/**< Context structure. */
};

static int ec_rtdm_open(struct rtdm_fd *fd, int oflags)
{
	struct ec_rtdm_context *ctx = rtdm_fd_to_private(fd);
#if DEBUG_RTDM
	struct rtdm_device *dev = rtdm_fd_device(fd);
	ec_rtdm_dev_t *rtdm_dev = dev->device_data;
#endif

	ctx->fd = fd;

	ctx->ioctl_ctx.writable = oflags & O_WRONLY || oflags & O_RDWR;
	ctx->ioctl_ctx.requested = 0;
	ctx->ioctl_ctx.process_data = NULL;
	ctx->ioctl_ctx.process_data_size = 0;

#if DEBUG_RTDM
	EC_MASTER_INFO(rtdm_dev->master, "RTDM device %s opened.\n",
			dev->name);
#endif

	return 0;
}

static void ec_rtdm_close(struct rtdm_fd *fd)
{
	struct ec_rtdm_context *ctx = rtdm_fd_to_private(fd);
	struct rtdm_device *dev = rtdm_fd_device(fd);
	ec_rtdm_dev_t *rtdm_dev = dev->device_data;

	if (ctx->ioctl_ctx.requested)
		ecrt_release_master(rtdm_dev->master);

	if (ctx->ioctl_ctx.process_data)
		vfree(ctx->ioctl_ctx.process_data);

#if DEBUG_RTDM
	EC_MASTER_INFO(rtdm_dev->master, "RTDM device %s closed.\n",
			dev->name);
#endif
}

#if DEBUG_RTDM
struct ec_ioctl_desc {
	unsigned int cmd;
	const char *name;
};

#define EC_IOCTL_DEF(ioctl)	\
	[_IOC_NR(ioctl)] = {	\
		.cmd = ioctl,	\
		.name = #ioctl	\
	}

static const struct ec_ioctl_desc ec_ioctls[] = {
	EC_IOCTL_DEF(EC_IOCTL_MODULE),
	EC_IOCTL_DEF(EC_IOCTL_MASTER),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SYNC),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SYNC_PDO),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SYNC_PDO_ENTRY),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_FMMU),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_DATA),
	EC_IOCTL_DEF(EC_IOCTL_MASTER_DEBUG),
	EC_IOCTL_DEF(EC_IOCTL_MASTER_RESCAN),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_STATE),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SDO),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SDO_ENTRY),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SDO_UPLOAD),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SDO_DOWNLOAD),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SII_READ),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SII_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_REG_READ),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_REG_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_FOE_READ),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_FOE_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SOE_READ),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_SOE_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_EOE_IP_PARAM),
	EC_IOCTL_DEF(EC_IOCTL_CONFIG),
	EC_IOCTL_DEF(EC_IOCTL_CONFIG_PDO),
	EC_IOCTL_DEF(EC_IOCTL_CONFIG_PDO_ENTRY),
	EC_IOCTL_DEF(EC_IOCTL_CONFIG_SDO),
	EC_IOCTL_DEF(EC_IOCTL_CONFIG_IDN),
#ifdef EC_EOE
	EC_IOCTL_DEF(EC_IOCTL_EOE_HANDLER),
#endif
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_DICT_UPLOAD),
	EC_IOCTL_DEF(EC_IOCTL_REQUEST),
	EC_IOCTL_DEF(EC_IOCTL_CREATE_DOMAIN),
	EC_IOCTL_DEF(EC_IOCTL_CREATE_SLAVE_CONFIG),
	EC_IOCTL_DEF(EC_IOCTL_SELECT_REF_CLOCK),
	EC_IOCTL_DEF(EC_IOCTL_ACTIVATE),
	EC_IOCTL_DEF(EC_IOCTL_DEACTIVATE),
	EC_IOCTL_DEF(EC_IOCTL_SEND),
	EC_IOCTL_DEF(EC_IOCTL_RECEIVE),
	EC_IOCTL_DEF(EC_IOCTL_MASTER_STATE),
	EC_IOCTL_DEF(EC_IOCTL_MASTER_LINK_STATE),
	EC_IOCTL_DEF(EC_IOCTL_APP_TIME),
	EC_IOCTL_DEF(EC_IOCTL_SYNC_REF),
	EC_IOCTL_DEF(EC_IOCTL_SYNC_SLAVES),
	EC_IOCTL_DEF(EC_IOCTL_REF_CLOCK_TIME),
	EC_IOCTL_DEF(EC_IOCTL_SYNC_MON_QUEUE),
	EC_IOCTL_DEF(EC_IOCTL_SYNC_MON_PROCESS),
	EC_IOCTL_DEF(EC_IOCTL_RESET),
	EC_IOCTL_DEF(EC_IOCTL_SC_SYNC),
	EC_IOCTL_DEF(EC_IOCTL_SC_WATCHDOG),
	EC_IOCTL_DEF(EC_IOCTL_SC_ADD_PDO),
	EC_IOCTL_DEF(EC_IOCTL_SC_CLEAR_PDOS),
	EC_IOCTL_DEF(EC_IOCTL_SC_ADD_ENTRY),
	EC_IOCTL_DEF(EC_IOCTL_SC_CLEAR_ENTRIES),
	EC_IOCTL_DEF(EC_IOCTL_SC_REG_PDO_ENTRY),
	EC_IOCTL_DEF(EC_IOCTL_SC_REG_PDO_POS),
	EC_IOCTL_DEF(EC_IOCTL_SC_DC),
	EC_IOCTL_DEF(EC_IOCTL_SC_SDO),
	EC_IOCTL_DEF(EC_IOCTL_SC_EMERG_SIZE),
	EC_IOCTL_DEF(EC_IOCTL_SC_EMERG_POP),
	EC_IOCTL_DEF(EC_IOCTL_SC_EMERG_CLEAR),
	EC_IOCTL_DEF(EC_IOCTL_SC_EMERG_OVERRUNS),
	EC_IOCTL_DEF(EC_IOCTL_SC_SDO_REQUEST),
	EC_IOCTL_DEF(EC_IOCTL_SC_REG_REQUEST),
	EC_IOCTL_DEF(EC_IOCTL_SC_VOE),
	EC_IOCTL_DEF(EC_IOCTL_SC_STATE),
	EC_IOCTL_DEF(EC_IOCTL_SC_IDN),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_SIZE),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_OFFSET),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_PROCESS),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_QUEUE),
	EC_IOCTL_DEF(EC_IOCTL_DOMAIN_STATE),
	EC_IOCTL_DEF(EC_IOCTL_SDO_REQUEST_INDEX),
	EC_IOCTL_DEF(EC_IOCTL_SDO_REQUEST_TIMEOUT),
	EC_IOCTL_DEF(EC_IOCTL_SDO_REQUEST_STATE),
	EC_IOCTL_DEF(EC_IOCTL_SDO_REQUEST_READ),
	EC_IOCTL_DEF(EC_IOCTL_SDO_REQUEST_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_SDO_REQUEST_DATA),
	EC_IOCTL_DEF(EC_IOCTL_REG_REQUEST_DATA),
	EC_IOCTL_DEF(EC_IOCTL_REG_REQUEST_STATE),
	EC_IOCTL_DEF(EC_IOCTL_REG_REQUEST_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_REG_REQUEST_READ),
	EC_IOCTL_DEF(EC_IOCTL_VOE_SEND_HEADER),
	EC_IOCTL_DEF(EC_IOCTL_VOE_REC_HEADER),
	EC_IOCTL_DEF(EC_IOCTL_VOE_READ),
	EC_IOCTL_DEF(EC_IOCTL_VOE_READ_NOSYNC),
	EC_IOCTL_DEF(EC_IOCTL_VOE_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_VOE_EXEC),
	EC_IOCTL_DEF(EC_IOCTL_VOE_DATA),
	EC_IOCTL_DEF(EC_IOCTL_SET_SEND_INTERVAL),
	EC_IOCTL_DEF(EC_IOCTL_SC_OVERLAPPING_IO),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_REBOOT),
	EC_IOCTL_DEF(EC_IOCTL_SLAVE_REG_READWRITE),
	EC_IOCTL_DEF(EC_IOCTL_REG_REQUEST_READWRITE),
	EC_IOCTL_DEF(EC_IOCTL_SETUP_DOMAIN_MEMORY),
	EC_IOCTL_DEF(EC_IOCTL_DEACTIVATE_SLAVES),
	EC_IOCTL_DEF(EC_IOCTL_64_REF_CLK_TIME_QUEUE),
	EC_IOCTL_DEF(EC_IOCTL_64_REF_CLK_TIME),
	EC_IOCTL_DEF(EC_IOCTL_SC_FOE_REQUEST),
	EC_IOCTL_DEF(EC_IOCTL_FOE_REQUEST_FILE),
	EC_IOCTL_DEF(EC_IOCTL_FOE_REQUEST_TIMEOUT),
	EC_IOCTL_DEF(EC_IOCTL_FOE_REQUEST_STATE),
	EC_IOCTL_DEF(EC_IOCTL_FOE_REQUEST_READ),
	EC_IOCTL_DEF(EC_IOCTL_FOE_REQUEST_WRITE),
	EC_IOCTL_DEF(EC_IOCTL_FOE_REQUEST_DATA),
	EC_IOCTL_DEF(EC_IOCTL_RT_SLAVE_REQUESTS),
	EC_IOCTL_DEF(EC_IOCTL_EXEC_SLAVE_REQUESTS),
};
#endif

static int ec_rtdm_ioctl_rt(struct rtdm_fd *fd, unsigned int request,
			 void __user *arg)
{
	struct ec_rtdm_context *ctx = rtdm_fd_to_private(fd);
	struct rtdm_device *dev = rtdm_fd_device(fd);
	ec_rtdm_dev_t *rtdm_dev = dev->device_data;

#if DEBUG_RTDM
	unsigned int nr = _IOC_NR(request);
	const struct ec_ioctl_desc *ioctl = &ec_ioctls[nr];

	EC_MASTER_INFO(rtdm_dev->master, "ioctl_rt(request = %u, ctl = %02x %s)"
			" on RTDM device %s.\n", request, _IOC_NR(request),ioctl->name,
			dev->name);
#endif

	/*
	 * FIXME: Execute ioctls from non-rt context except below ioctls to
	 *	  avoid any unknown system hanging.
	 */
	switch (request) {
	case EC_IOCTL_SEND:
	case EC_IOCTL_RECEIVE:
	case EC_IOCTL_MASTER_STATE:
	case EC_IOCTL_APP_TIME:
	case EC_IOCTL_SYNC_REF:
	case EC_IOCTL_SYNC_SLAVES:
	case EC_IOCTL_REF_CLOCK_TIME:
	case EC_IOCTL_SC_STATE:
	case EC_IOCTL_DOMAIN_PROCESS:
	case EC_IOCTL_DOMAIN_QUEUE:
	case EC_IOCTL_DOMAIN_STATE:
		break;
	default:
		return -ENOSYS;
	}

	return ec_ioctl_rtdm(rtdm_dev->master, &ctx->ioctl_ctx, request, arg);
}

static int ec_rtdm_ioctl(struct rtdm_fd *fd, unsigned int request,
			 void __user *arg)
{
	struct ec_rtdm_context *ctx = rtdm_fd_to_private(fd);
	struct rtdm_device *dev = rtdm_fd_device(fd);
	ec_rtdm_dev_t *rtdm_dev = dev->device_data;

#if DEBUG_RTDM
	unsigned int nr = _IOC_NR(request);
	const struct ec_ioctl_desc *ioctl = &ec_ioctls[nr];

	EC_MASTER_INFO(rtdm_dev->master, "ioctl(request = %u, ctl = %02x %s)"
			" on RTDM device %s.\n", request, _IOC_NR(request),ioctl->name,
			dev->name);
#endif

	return ec_ioctl_rtdm(rtdm_dev->master, &ctx->ioctl_ctx, request, arg);
}

static struct rtdm_driver ec_rtdm_driver = {
	.profile_info		= RTDM_PROFILE_INFO(ec_rtdm,
						    RTDM_CLASS_EXPERIMENTAL,
						    222,
						    0),
	.device_flags		= RTDM_NAMED_DEVICE,
	.device_count		= 1,
	.context_size		= sizeof(struct ec_rtdm_context),
	.ops = {
		.open		= ec_rtdm_open,
		.close		= ec_rtdm_close,
		.ioctl_rt	= ec_rtdm_ioctl_rt,
		.ioctl_nrt	= ec_rtdm_ioctl,
	},
};

int ec_rtdm_dev_init(ec_rtdm_dev_t *rtdm_dev, ec_master_t *master)
{
	struct rtdm_device *dev;
	int ret;

	rtdm_dev->master = master;

	rtdm_dev->dev = kzalloc(sizeof(struct rtdm_device), GFP_KERNEL);
	if (!rtdm_dev->dev) {
		EC_MASTER_ERR(master,
				"Failed to reserve memory for RTDM device.\n");
		return -ENOMEM;
	}

	dev = rtdm_dev->dev;

	dev->driver = &ec_rtdm_driver;
	dev->device_data = rtdm_dev;
	dev->label = "EtherCAT%u";

	ret = rtdm_dev_register(dev);
	if (ret) {
		EC_MASTER_ERR(master, "Initialization of RTDM interface failed"
				" (return value %i).\n", ret);
		kfree(dev);
		return ret;
	}

	EC_MASTER_INFO(master, "Registered RTDM device %s.\n", dev->name);

	return 0;
}

void ec_rtdm_dev_clear(ec_rtdm_dev_t *rtdm_dev)
{
	rtdm_dev_unregister(rtdm_dev->dev);

	EC_MASTER_INFO(rtdm_dev->master, "Unregistered RTDM device %s.\n",
			rtdm_dev->dev->name);

	kfree(rtdm_dev->dev);
}

int ec_rtdm_mmap(ec_ioctl_context_t *ioctl_ctx, void **user_address)
{
	struct ec_rtdm_context *ctx =
		container_of(ioctl_ctx, struct ec_rtdm_context, ioctl_ctx);
	int ret;

	ret = rtdm_mmap_to_user(ctx->fd,
			ioctl_ctx->process_data, ioctl_ctx->process_data_size,
			PROT_READ | PROT_WRITE,
			user_address,
			NULL, NULL);
	if (ret < 0)
		return ret;

	return 0;
}
