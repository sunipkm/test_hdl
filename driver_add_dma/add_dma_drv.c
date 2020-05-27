/* ADD DMA DRV
 *
 * This module is designed to be a small example of a DMA device driver that is
 * a client to the DMA Engine using the AXI DMA driver. It serves as a proxy for
 * kernel space DMA control to a user space application.
 *
 * A zero copy scheme is provided by allowing user space to mmap a kernel allocated
 * memory region into user space, referred to as a proxy channel interface. The
 * ioctl function is provided to start a DMA transfer which then blocks until the
 * transfer is complete. No input arguments are being used in the ioctl function.
 *
 * There is an associated user space application, add_dma_test.c, and add_dma_drv.h
 * that works with this device driver.
 *
 * The hardware design was tested with an AXI DMA without scatter gather and
 * with the transmit channel looped back to the receive channel.
 *
 * This driver is character driver which creates 2 devices that user space can
 * access for each DMA channel, /dev/add_dma_in and /dev/add_dma_out.

 * An internal test mode is provided to allow it to be self testing without the
 * need for a user space application and this mode is good for making bigger
 * changes to this driver.
 *
 * This driver is designed to be simple to help users get familiar with how to
 * use the DMA driver provided by Xilinx which uses the Linux DMA Engine. The
 * performance of this example is not expected to be high without more work.
 *
 * To use this driver a node must be added into the device tree.  Add the
 * following node while adjusting the dmas property to match the name of
 * the AXI DMA node.
 *
 * add_dma {
 *    compatible ="xlnx,add_dma";
 *    dmas = <&axi_dma_0 0
 *            &axi_dma_0 1>;
 *    dma-names = "add_dma_out", "add_dma_in";
 * }
 *
 */

#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of_dma.h>

#include "add_dma_drv.h"

#undef INTERNAL_TEST

MODULE_LICENSE("GPL");

#define DRIVER_NAME "add_dma: "
#define CHANNEL_COUNT 2
#define TX_CHANNEL 0
#define RX_CHANNEL 1
#define ERROR -1
#define NOT_LAST_CHANNEL 0
#define LAST_CHANNEL 1

/* The following module parameter controls if the internal test runs when the module is inserted.
 */
static unsigned internal_test = 0;
module_param(internal_test, int, S_IRUGO);

/* The following data structure represents a single channel of DMA, transmit or receive in the case
 * when using AXI DMA.  It contains all the data to be maintained for the channel.
 */
struct add_dma_channel
{
	struct add_dma_channel_interface *interface_p; /* user to kernel space interface */
	dma_addr_t interface_phys_addr;

	struct device *proxy_device_p; /* character device support */
	struct device *dma_device_p;
	dev_t dev_node;
	struct cdev cdev;
	struct class *class_p;

	struct dma_chan *channel_p; /* dma support */
	struct completion cmp;
	dma_cookie_t cookie;
	dma_addr_t dma_handle;
	u32 direction; /* DMA_MEM_TO_DEV or DMA_DEV_TO_MEM */
};

/* Allocate the channels for this example statically rather than dynamically for simplicity.
 */
static struct add_dma_channel channels[CHANNEL_COUNT];

/* Handle a callback and indicate the DMA transfer is complete to another
 * thread of control
 */
static void sync_callback(void *completion)
{
	/* Indicate the DMA transaction completed to allow the other
	 * thread of control to finish processing
	 */
	complete(completion);
}

/* Prepare a DMA buffer to be used in a DMA transaction, submit it to the DMA engine
 * to ibe queued and return a cookie that can be used to track that status of the
 * transaction
 */
static void start_transfer(struct add_dma_channel *pchannel_p)
{
	if (pchannel_p->direction == DMA_DEV_TO_MEM) // rx channel only, configure the ADD DMA IP
	{
		void *add_ip_reg = ioremap_nocache(0x43c00000, 0x10000); // map 64k of address space
		printk(KERN_INFO DRIVER_NAME "Setting up ADD DMA IP: Unique ID %d\n", ioread32(add_ip_reg + 0xc)); // verify unique ID
		// reset the system
		iowrite32(0x1, add_ip_reg + 0x0);
		// set the length
		iowrite32(pchannel_p->interface_p->length, add_ip_reg + 0x8);
		// print status
		printk(KERN_INFO DRIVER_NAME "Setting up ADD DMA IP: Length set to %d, requested %d\n", ioread32(add_ip_reg + 0x8), pchannel_p->interface_p->length); // verify length
		// enable
		printk(KERN_INFO DRIVER_NAME "Reset: 0x%01x Enable: 0x%01x Length: %d UID: %d\n", ioread32(add_ip_reg + 0x0), ioread32(add_ip_reg + 0x4), ioread32(add_ip_reg + 0x8), ioread32(add_ip_reg + 0xc));
		iounmap(add_ip_reg);
	}
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	struct dma_async_tx_descriptor *chan_desc;
	struct add_dma_channel_interface *interface_p = pchannel_p->interface_p;
	struct dma_device *dma_device = pchannel_p->channel_p->device;
	/* For now use a single entry in a scatter gather list just for future
	 * flexibility for scatter gather.
	 */
	// sg_init_table(&pchannel_p->sglist, 1);
	// sg_dma_address(&pchannel_p->sglist) = pchannel_p->dma_handle;
	// sg_dma_len(&pchannel_p->sglist) = interface_p->length;

	/* Step 5, create a buffer (channel)  descriptor for the buffer since only a  
	 * single buffer is being used for this transfer
	 */
	chan_desc = dmaengine_prep_slave_single(pchannel_p->channel_p, pchannel_p->interface_phys_addr, interface_p->length, pchannel_p->direction, flags);

	// chan_desc = dma_device->device_prep_slave_sg(pchannel_p->channel_p, &pchannel_p->sglist, 1,
	// 											 pchannel_p->direction, flags, NULL);

	/* Make sure the operation was completed successfully
	 */
	if (!chan_desc)
	{
		printk(KERN_ERR DRIVER_NAME "dmaengine_prep*() error\n");
	}
	else
	{
		chan_desc->callback = sync_callback;
		chan_desc->callback_param = &pchannel_p->cmp;

		/* Initialize the completion for the transfer and before using it
		 * then submit the transaction to the DMA engine so that it's queued
		 * up to be processed later and get a cookie to track it's status
		 */

		pchannel_p->cookie = dmaengine_submit(chan_desc);
		if (dma_submit_error(pchannel_p->cookie))
		{
			printk(KERN_ERR DRIVER_NAME "Submit error\n");
			return;
		}
		init_completion(&pchannel_p->cmp);
		/* Start the DMA transaction which was previously queued up in the DMA engine
		 */
		dma_async_issue_pending(pchannel_p->channel_p);
	}
}

/* Wait for a DMA transfer that was previously submitted to the DMA engine
 */
static void wait_for_transfer(struct add_dma_channel *pchannel_p)
{
	unsigned long timeout = msecs_to_jiffies(10000);
	printk(KERN_INFO DRIVER_NAME "Transfer on channel %s: Timeout set for %d\n", pchannel_p->direction == DMA_DEV_TO_MEM ? "TX" : "RX", timeout);
	enum dma_status status;

	pchannel_p->interface_p->status = PROXY_BUSY;

	/* Wait for the transaction to complete, or timeout, or get an error
	 */
	timeout = wait_for_completion_timeout(&pchannel_p->cmp, timeout);
	printk(KERN_INFO DRIVER_NAME "Transfer on channel %s: Timeout after wait %d\n", pchannel_p->direction == DMA_DEV_TO_MEM ? "TX" : "RX", timeout);
	status = dma_async_is_tx_complete(pchannel_p->channel_p, pchannel_p->cookie, NULL, NULL);
	printk(KERN_INFO DRIVER_NAME "Transfer on channel %s: Status = %d\n", pchannel_p->direction == DMA_DEV_TO_MEM ? "TX" : "RX", status);
	// status = DMA_COMPLETE;

	if (timeout == 0)
	{
		pchannel_p->interface_p->status = PROXY_TIMEOUT;
		printk(KERN_ERR DRIVER_NAME "%s DMA timed out\n", pchannel_p->direction == DMA_DEV_TO_MEM ? "TX" : "RX");
	}
	else
	if (status != DMA_COMPLETE)
	{
		pchannel_p->interface_p->status = PROXY_ERROR;
		printk(KERN_ERR DRIVER_NAME "DMA returned completion callback status of: %s\n",
			   status == DMA_ERROR ? "error" : "in progress");
	}
	else
	{
		pchannel_p->interface_p->status = PROXY_NO_ERROR;
		printk(KERN_INFO DRIVER_NAME " DMA transfer complete\n");
	}
}

/* Perform the DMA transfer for the channel, starting it then blocking to wait for completion.
 */
static void transfer(struct add_dma_channel *pchannel_p)
{
	/* The physical address of the buffer in the interface is needed for the dma transfer
	 * as the buffer may not be the first data in the interface
	 */
	pchannel_p->dma_handle = (dma_addr_t)(pchannel_p->interface_phys_addr +
										  offsetof(struct add_dma_channel_interface, buffer));

	start_transfer(pchannel_p);
	// if (pchannel_p->direction == DMA_DEV_TO_MEM) // wait for transfer on tx only
	wait_for_transfer(pchannel_p);
}

/* The following functions are designed to test the driver from within the device
 * driver without any user space.
 */
static void tx_test(struct work_struct *unused)
{
	channels[TX_CHANNEL].interface_p->length = TEST_SIZE;
	transfer(&channels[TX_CHANNEL]);
}
static void test(void)
{
	int i;
	struct work_struct work;

	printk(DRIVER_NAME "Starting internal test\n");

	/* Initialize the buffers for the test
	 */
	for (i = 0; i < TEST_SIZE; i++)
	{
		channels[TX_CHANNEL].interface_p->buffer[i] = i;
		channels[RX_CHANNEL].interface_p->buffer[i] = 0;
	}

	/* Since the transfer function is blocking the transmit channel is started from a worker
	 * thread
	 */
	INIT_WORK(&work, tx_test);
	schedule_work(&work);

	/* Receive the data that was just sent and looped back (in this case TEST_SIZE is small enough that the effect is similar to loopback)
	 */
	channels[1].interface_p->length = TEST_SIZE;
	transfer(&channels[1]);

	/* Verify the receiver buffer matches the transmit buffer to
	 * verify the transfer was good
	 */
	for (i = 0; i < TEST_SIZE; i++)
		if (channels[TX_CHANNEL].interface_p->buffer[i] !=
			channels[RX_CHANNEL].interface_p->buffer[i])
		{
			printk(DRIVER_NAME "buffers not equal, first index = %d\n", i);
			break;
		}

	printk(DRIVER_NAME "Internal test complete\n");
}

/* Map the memory for the channel interface into user space such that user space can
 * access it using coherent memory which will be non-cached for s/w coherent systems
 * such as Zynq 7K or the current default for Zynq MPSOC. MPSOC can be h/w coherent
 * when set up and then the memory will be cached.
 */
static int mmap(struct file *file_p, struct vm_area_struct *vma)
{
	struct add_dma_channel *pchannel_p = (struct add_dma_channel *)file_p->private_data;

	return dma_mmap_coherent(pchannel_p->dma_device_p, vma,
							 pchannel_p->interface_p, pchannel_p->interface_phys_addr,
							 vma->vm_end - vma->vm_start);
}

/* Open the device file and set up the data pointer to the proxy channel data for the
 * proxy channel such that the ioctl function can access the data structure later.
 */
static int local_open(struct inode *ino, struct file *file)
{
	file->private_data = container_of(ino->i_cdev, struct add_dma_channel, cdev);

	return 0;
}

/* Close the file and there's nothing to do for it
 */
static int release(struct inode *ino, struct file *file)
{
	struct add_dma_channel *pchannel_p = (struct add_dma_channel *)file->private_data;
	struct dma_device *dma_device = pchannel_p->channel_p->device;

	/* Stop all the activity when the channel is closed assuming this
	 * may help if the application is aborted without normal closure
	 */

	/*dma_device->device_terminate_all(pchannel_p->channel_p);*/
	return 0;
}

/* Perform I/O control to start a DMA transfer.
 */
static long ioctl(struct file *file, unsigned int unused, unsigned long arg)
{
	struct add_dma_channel *pchannel_p = (struct add_dma_channel *)file->private_data;

	/* Perform the DMA transfer on the specified channel blocking til it completes
	 */
	transfer(pchannel_p);

	return 0;
}
static struct file_operations dm_fops = {
	.owner = THIS_MODULE,
	.open = local_open,
	.release = release,
	.unlocked_ioctl = ioctl,
	.mmap = mmap};

/* Initialize the driver to be a character device such that is responds to
 * file operations.
 */
static int cdevice_init(struct add_dma_channel *pchannel_p, char *name)
{
	int rc;
	char device_name[32] = "add_dma";
	static struct class *local_class_p = NULL;

	/* Allocate a character device from the kernel for this driver.
	 */
	rc = alloc_chrdev_region(&pchannel_p->dev_node, 0, 1, "add_dma");

	if (rc)
	{
		dev_err(pchannel_p->dma_device_p, "unable to get a char device number\n");
		return rc;
	}

	/* Initialize the device data structure before registering the characterdma_proxy
	 * device with the kernel.
	 */
	cdev_init(&pchannel_p->cdev, &dm_fops);
	pchannel_p->cdev.owner = THIS_MODULE;
	rc = cdev_add(&pchannel_p->cdev, pchannel_p->dev_node, 1);

	if (rc)
	{
		dev_err(pchannel_p->dma_device_p, "unable to add char device\n");
		goto init_error1;
	}

	/* Only one class in sysfs is to be created for multiple channels,
	 * create the device in sysfs which will allow the device node
	 * in /dev to be created
	 */
	if (!local_class_p)
	{
		local_class_p = class_create(THIS_MODULE, DRIVER_NAME);

		if (IS_ERR(pchannel_p->dma_device_p->class))
		{
			dev_err(pchannel_p->dma_device_p, "unable to create class\n");
			rc = ERROR;
			goto init_error2;
		}
	}
	pchannel_p->class_p = local_class_p;

	/* Create the device node in /dev so the device is accessible
	 * as a character device
	 */
	strcat(device_name, name);
	pchannel_p->proxy_device_p = device_create(pchannel_p->class_p, NULL,
											   pchannel_p->dev_node, NULL, name);

	if (IS_ERR(pchannel_p->proxy_device_p))
	{
		dev_err(pchannel_p->dma_device_p, "unable to create the device\n");
		goto init_error3;
	}

	return 0;

init_error3:
	class_destroy(pchannel_p->class_p);

init_error2:
	cdev_del(&pchannel_p->cdev);

init_error1:
	unregister_chrdev_region(pchannel_p->dev_node, 1);
	return rc;
}

/* Exit the character device by freeing up the resources that it created and
 * disconnecting itself from the kernel.
 */
static void cdevice_exit(struct add_dma_channel *pchannel_p, int last_channel)
{
	/* Take everything down in the reverse order
	 * from how it was created for the char device
	 */
	if (pchannel_p->proxy_device_p)
	{
		device_destroy(pchannel_p->class_p, pchannel_p->dev_node);
		if (last_channel)
			class_destroy(pchannel_p->class_p);

		cdev_del(&pchannel_p->cdev);
		unregister_chrdev_region(pchannel_p->dev_node, 1);
	}
}

/* Create a DMA channel by getting a DMA channel from the DMA Engine and then setting
 * up the channel as a character device to allow user space control.
 */
static int create_channel(struct platform_device *pdev, struct add_dma_channel *pchannel_p, char *name, u32 direction)
{
	int rc;

	/* Request the DMA channel from the DMA engine and then use the device from
	 * the channel for the proxy channel also.
	 */
	pchannel_p->channel_p = dma_request_slave_channel(&pdev->dev, name);
	if (!pchannel_p->channel_p)
	{
		dev_err(pchannel_p->dma_device_p, "DMA channel request error\n");
		return ERROR;
	}
	pchannel_p->dma_device_p = &pdev->dev;

	/* Initialize the character device for the dma proxy channel
	 */
	rc = cdevice_init(pchannel_p, name);
	if (rc)
		return rc;

	pchannel_p->direction = direction;

	/* Allocate DMA memory for the proxy channel interface.
	 */
	pchannel_p->interface_p = (struct add_dma_channel_interface *)
		dmam_alloc_coherent(pchannel_p->dma_device_p,
							sizeof(struct add_dma_channel_interface),
							&pchannel_p->interface_phys_addr, GFP_KERNEL);
	printk(KERN_INFO DRIVER_NAME "Allocating uncached memory at virtual address 0x%p, physical address 0x%p\n",
		   pchannel_p->interface_p, (void *)pchannel_p->interface_phys_addr);

	if (!pchannel_p->interface_p)
	{
		dev_err(pchannel_p->dma_device_p, "DMA allocation error\n");
		return ERROR;
	}

	/* Reset the add interface
	 * Register map: refer to IP documentation
	 */ 
	// void *add_ip_addr = ioremap_nocache(0x43c00000, 0xffff); // 64K memory region
	// printk(KERN_INFO DRIVER_NAME " : ADD DMA IP UID: %d\n", ioread32(add_ip_addr + 0xc));
	// printk(KERN_INFO DRIVER_NAME " : Resetting ADD DMA IP\n");
	// iowrite32(0x1, add_ip_addr);
	// printk(KERN_INFO DRIVER_NAME " : Register callback 0x%01x 0x%01x 0x%04x %d\n", ioread32(add_ip_addr), ioread32(add_ip_addr + 0x4), ioread32(add_ip_addr + 0x8), ioread32(add_ip_addr + 0xc));
	// printk(KERN_INFO DRIVER_NAME " : Setting length = 0x%04x\n", TEST_SIZE);
	// iowrite32(TEST_SIZE, add_ip_addr + 0x8);
	// printk(KERN_INFO DRIVER_NAME " : Register callback 0x%01x 0x%01x 0x%04x %d\n", ioread32(add_ip_addr), ioread32(add_ip_addr + 0x4), ioread32(add_ip_addr + 0x8), ioread32(add_ip_addr + 0xc));
	// printk(KERN_INFO DRIVER_NAME " : Enabling\n");
	// iowrite32(1, add_ip_addr + 0x4);
	// printk(KERN_INFO DRIVER_NAME " : Register callback 0x%01x 0x%01x 0x%04x %d\n", ioread32(add_ip_addr), ioread32(add_ip_addr + 0x4), ioread32(add_ip_addr + 0x8), ioread32(add_ip_addr + 0xc));
	// iounmap(add_ip_addr);
	return 0;
}

/* Initialize the dma proxy device driver module.
 */
static int add_dma_probe(struct platform_device *pdev)
{
	int rc;

	// void *reg_addr = ioremap_nocache(0x43c00000, 0xffff);

	printk(KERN_INFO "add_dma module initialized\n");

	/* Create the transmit and receive channels.
	 */
	rc = create_channel(pdev, &channels[TX_CHANNEL], "add_dma_out", DMA_MEM_TO_DEV);

	if (rc)
		return rc;

	rc = create_channel(pdev, &channels[RX_CHANNEL], "add_dma_in", DMA_DEV_TO_MEM);
	if (rc)
		return rc;

	if (internal_test)
		test();

	// init the adder

	// printk(KERN_INFO DRIVER_NAME " Unique ID: %d\n", readl((void *)0x43c00000)); //ioread32(reg_addr + 0xc));
	return 0;
}

/* Exit the dma proxy device driver module.
 */
static int add_dma_remove(struct platform_device *pdev)
{
	int i;

	printk(KERN_INFO "add_dma module exited\n");

	/* Take care of the char device infrastructure for each
	 * channel except for the last channel. Handle the last
	 * channel seperately.
	 */
	for (i = 0; i < CHANNEL_COUNT - 1; i++)
		if (channels[i].proxy_device_p)
			cdevice_exit(&channels[i], NOT_LAST_CHANNEL);

	cdevice_exit(&channels[i], LAST_CHANNEL);

	/* Take care of the DMA channels and the any buffers allocated
	 * for the DMA transfers. The DMA buffers are using managed
	 * memory such that it's automatically done.
	 */
	for (i = 0; i < CHANNEL_COUNT; i++)
		if (channels[i].channel_p)
		{
			channels[i].channel_p->device->device_terminate_all(channels[i].channel_p);
			dma_release_channel(channels[i].channel_p);
		}
	return 0;
}

static const struct of_device_id add_dma_of_ids[] = {
	{
		.compatible = "xlnx,add_dma",
	},
	{}};

static struct platform_driver add_dma_driver = {
	.driver = {
		.name = "add_dma_driver",
		.owner = THIS_MODULE,
		.of_match_table = add_dma_of_ids,
	},
	.probe = add_dma_probe,
	.remove = add_dma_remove,
};

static int __init add_dma_init(void)
{
	return platform_driver_register(&add_dma_driver);
}

static void __exit add_dma_exit(void)
{
	platform_driver_unregister(&add_dma_driver);
}

module_init(add_dma_init)
	module_exit(add_dma_exit)

		MODULE_AUTHOR("Xilinx, Inc., SKM");
MODULE_DESCRIPTION("ADD DMA Prototype");
MODULE_LICENSE("GPL v2");
