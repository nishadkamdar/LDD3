/*
 * Sample disk driver, from the beginning.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/timer.h>
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* invalidate_bdev */
#include <linux/bio.h>

MODULE_LICENSE("Dual BSD/GPL");

#define SBULL_MAJOR 0       /* dynamic major by default */
#define SBULL_DEVS 2        /* two disks */
#define SBULL_RAHEAD 2      /* two sectors */
#define SBULL_SIZE 2048     /* two megs each */
#define SBULL_BLKSIZE 1024  /* 1k blocks */
#define SBULL_HARDSECT 512  /* 2.2 and 2.4 can used different values */

#define SBULLR_MAJOR 0      /* Dynamic major for raw device */

static int sbull_major = 0;
module_param(sbull_major, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);
static int nsectors = 1024;	/* How big the drive is */
module_param(nsectors, int, 0);
static int ndevices = 1;
module_param(ndevices, int, 0);

/*
 * The different "request modes" we can use.
 */
enum {
	RM_SIMPLE  = 0,	/* The extra-simple request function */
	RM_FULL    = 1,	/* The full-blown version */
	RM_NOQUEUE = 2,	/* Use make_request */
};
static int request_mode = RM_FULL;
module_param(request_mode, int, 0);

/*
 * Minor number and partition management.
 */
//#define SBULL_MINORS	16
#define SBULL_MINORS	1
#define MINOR_SHIFT	4
#define DEVNUM(kdevnum)	(MINOR(kdev_t_to_nr(kdevnum)) >> MINOR_SHIFT

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE	512

/*
 * After this much idle time, the driver will simulate a media change.
 */
#define INVALIDATE_DELAY	30*HZ

/*
 * The internal representation of our device.
 */
struct sbull_dev {
        int size;                       /* Device size in sectors */
        u8 *data;                       /* The data array */
        short users;                    /* How many users */
        short media_change;             /* Flag a media change? */
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
        struct timer_list timer;        /* For simulated media changes */
};

static struct sbull_dev *Devices = NULL;

struct sbull_dev *t_dev = NULL;
/*
 * Handle an I/O request.
 */
static void sbull_transfer(struct sbull_dev *dev, unsigned long start,
		unsigned long len, char *buffer, int write)
{
	unsigned long offset = start*KERNEL_SECTOR_SIZE;

	if ((offset + len) > dev->size) {
		printk (KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, len);
		return;
	}
	if (write)
		memcpy(dev->data + offset, buffer, len);
	else
		memcpy(buffer, dev->data + offset, len);
}

/*
 * Transfer a full request.
 */
static int sbull_xfer_request(struct sbull_dev *dev, struct request *req)
{
	struct bio_vec bvec;
	struct req_iterator iter;
	int nsect = 0;
    	sector_t start_sector = blk_rq_pos(req);
	unsigned int sector_cnt = blk_rq_sectors(req);
	unsigned int sector_offset = 0;
	unsigned int sectors = 0;
	unsigned char *buffer;
	int ret = 0;

	/*rq_for_each_segment(bvec, req, iter) {
		sector_t sector = iter.iter.bi_sector;
		char *buffer = kmap_atomic(bvec.bv_page);
		unsigned long offset = bvec.bv_offset;
		size_t len = bvec.bv_len;
		int dir = bio_data_dir(iter.bio);

		sbull_transfer(dev, sector, len, buffer + offset, dir);
		printk(KERN_NOTICE "Transferred %ld bytes successfully \n", len);

		kunmap_atomic(buffer);
		nsect += len/KERNEL_SECTOR_SIZE;
	}*/

	sector_offset = 0;
	rq_for_each_segment(bvec, req, iter)
	{
		printk(KERN_NOTICE "Transferring req for each segment \n");
		buffer = page_address(bvec.bv_page) + bvec.bv_offset;

		sectors = bvec.bv_len / KERNEL_SECTOR_SIZE;
		printk(KERN_DEBUG "sbull: Start Sector: %llu, Sector Offset: %llu; Buffer: %p; Length:%u sectors; Sectors in req: %u sectors\n",
				(unsigned long long)(start_sector), (unsigned long long)(sector_offset), buffer, sectors, sector_cnt);

		if (rq_data_dir(req))
			memcpy(dev->data + (start_sector + sector_offset) * KERNEL_SECTOR_SIZE, buffer, bvec.bv_len);
		else
			memcpy(buffer, dev->data + (start_sector + sector_offset) * KERNEL_SECTOR_SIZE, bvec.bv_len);
		
		sector_offset += sectors;
	}

	if (sector_offset != sector_cnt)
	{
		printk(KERN_ALERT "sbull: bio info doesn't match with the request info");
		ret = -EIO;
	}	

	return ret;
}

/*
 * Smarter request function that "handles clustering".
 */
static void sbull_full_request(struct request_queue *q)
{
	struct request *req;
	int ret;
	struct sbull_dev *dev = q->queuedata;

	while ((req = blk_fetch_request(q)) != NULL)
	{
		printk(KERN_NOTICE "Fetched new request\n");
		if (blk_rq_is_passthrough(req))
		{
			printk(KERN_NOTICE "Skip non-fs request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}		
		ret = sbull_xfer_request(dev, req);
		__blk_end_request_all(req, ret);
	}
}

/*
 * Open and close.
 */

static int sbull_open(struct block_device *bdev, fmode_t mode)
{
	struct sbull_dev *dev = bdev->bd_inode->i_bdev->bd_disk->private_data;
	printk(KERN_ALERT "Successfully opened device %s\n", dev->gd->disk_name);	

	del_timer_sync(&dev->timer);
	spin_lock(&dev->lock);
	if (! dev->users) 
		check_disk_change(bdev->bd_inode->i_bdev);
	dev->users++;
	spin_unlock(&dev->lock);
	return 0;
}

static void sbull_release(struct gendisk *gd, fmode_t mode)
{
	struct sbull_dev *dev = gd->private_data;
	
	t_dev = dev;

	spin_lock(&dev->lock);
	dev->users--;

	if (!dev->users) {
		dev->timer.expires = jiffies + INVALIDATE_DELAY;
		add_timer(&dev->timer);
	}
	spin_unlock(&dev->lock);
}

/*
 * Look for a (simulated) media change.
 */
int sbull_media_changed(struct gendisk *gd)
{
	struct sbull_dev *dev = gd->private_data;
	
	return dev->media_change;
}

/*
 * Revalidate.  WE DO NOT TAKE THE LOCK HERE, for fear of deadlocking
 * with open.  That needs to be reevaluated.
 */
int sbull_revalidate(struct gendisk *gd)
{
	struct sbull_dev *dev = gd->private_data;
	
	if (dev->media_change) {
		printk("media changed\n");
		dev->media_change = 0;
		memset (dev->data, 0, dev->size);
	}
	return 0;
}

/*
 * The "invalidate" function runs out of the device timer; it sets
 * a flag to simulate the removal of the media.
 */
void sbull_invalidate(struct timer_list *t)
{
	spin_lock(&t_dev->lock);
	if (t_dev->users || !t_dev->data) 
		printk (KERN_WARNING "sbull: timer sanity check failed\n");
	else
		t_dev->media_change = 1;
	spin_unlock(&t_dev->lock);
}

/*
 * The ioctl() implementation
 */

int sbull_ioctl (struct block_device *bdev, fmode_t mode,
                 unsigned int cmd, unsigned long arg)
{
	long size;
	struct hd_geometry geo;
	struct sbull_dev *dev = bdev->bd_inode->i_bdev->bd_disk->private_data;

	switch(cmd) {
	    case HDIO_GETGEO:
        	/*
		 * Get geometry: since we are a virtual device, we have to make
		 * up something plausible.  So we claim 16 sectors, four heads,
		 * and calculate the corresponding number of cylinders.  We set the
		 * start of data at sector four.
		 */
		size = dev->size*(hardsect_size/KERNEL_SECTOR_SIZE);
		geo.cylinders = (size & ~0x3f) >> 6;
		geo.heads = 4;
		geo.sectors = 16;
		geo.start = 4;
		if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
			return -EFAULT;
		return 0;
	}

	return -ENOTTY; /* unknown command */
}

/*
 * The device operations structure.
 */
static struct block_device_operations sbull_ops = {
	.owner           = THIS_MODULE,
	.open 	         = sbull_open,
	.release 	 = sbull_release,
	.media_changed   = sbull_media_changed,
	.revalidate_disk = sbull_revalidate,
	.ioctl	         = sbull_ioctl
};


/*
 * Set up our internal device.
 */
static void setup_device(struct sbull_dev *dev, int which)
{
	printk(KERN_ALERT "Setting up device\n");	
	/*
	 * Get some memory.
	 */
	memset (dev, 0, sizeof (struct sbull_dev));
	dev->size = nsectors*hardsect_size;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		printk (KERN_NOTICE "vmalloc failure.\n");
		return;
	}
	spin_lock_init(&dev->lock);
	
	/*
	 * The timer which "invalidates" the device.
	 */
	timer_setup(&dev->timer, sbull_invalidate, 0);
	printk(KERN_ALERT "timer initialized\n");	
	
	/*
	 * The I/O queue, depending on whether we are using our own
	 * make_request function or not.
	 */
	switch (request_mode) {
	    case RM_FULL:
		dev->queue = blk_init_queue(sbull_full_request, &dev->lock);
		if (dev->queue == NULL)
			goto out_vfree;
		
		printk(KERN_ALERT "queue initialized\n");	
		break;

	    default:
		printk(KERN_NOTICE "Bad request mode %d, using simple\n", request_mode);
        	/* fall into.. */
		break;
	}
	blk_queue_logical_block_size(dev->queue, hardsect_size);
	dev->queue->queuedata = dev;
	/*
	 * And the gendisk structure.
	 */
	dev->gd = alloc_disk(SBULL_MINORS);
	if (! dev->gd) {
		printk (KERN_NOTICE "alloc_disk failure\n");
		goto out_vfree;
	}
	printk(KERN_ALERT "allocated disk successfully\n");	
	dev->gd->major = sbull_major;
	dev->gd->first_minor = which*SBULL_MINORS;
	dev->gd->fops = &sbull_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf (dev->gd->disk_name, 32, "sbull%c", which + 'a');
	set_capacity(dev->gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);
	printk(KERN_ALERT "Successfully set up device %c\n", which + 'a');	
	return;

  out_vfree:
	if (dev->data)
		vfree(dev->data);
}

static int __init sbull_init(void)
{
	int i;
	/*
	 * Get registered.
	 */
	sbull_major = register_blkdev(sbull_major, "sbull");
	if (sbull_major <= 0) {
		printk(KERN_WARNING "sbull: unable to get major number\n");
		return -EBUSY;
	}
	/*
	 * Allocate the device array, and initialize each one.
	 */
	Devices = kmalloc(ndevices * sizeof(struct sbull_dev), GFP_KERNEL);
	printk(KERN_ALERT "Successfully allocated devices\n");	
	if (Devices == NULL)
		goto out_unregister;
	for (i = 0; i < ndevices; i++) 
		setup_device(Devices + i, i);
   
	printk(KERN_ALERT "Successfully set up device\n");	
	return 0;

  out_unregister:
	unregister_blkdev(sbull_major, "sbd");
	return -ENOMEM;
}

static void sbull_exit(void)
{
	int i;

	for (i = 0; i < ndevices; i++) {
		struct sbull_dev *dev = Devices + i;

		del_timer_sync(&dev->timer);
		if (dev->gd) {
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		if (dev->queue) {
			blk_cleanup_queue(dev->queue);
		}
		if (dev->data)
			vfree(dev->data);
	}
	unregister_blkdev(sbull_major, "sbull");
	kfree(Devices);
}
	
module_init(sbull_init);
module_exit(sbull_exit);
