/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/uaccess.h> // copy_from_user (and to)
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Rob Johnson");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
	
	// save device information
	struct aesd_dev *dev;
	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

	// No hardware to release so can return without action.

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	
	if(mutex_lock_interruptible(&aesd_device.lock) != 0) {
		// couldn't lock
		return -ERESTARTSYS;
	}

	size_t offset;	
	struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &offset); 
	size_t read_len;

	if(entry != NULL) { // data is valid
		size_t data_available = entry->size - offset;
		if(data_available > count) {
			read_len = count;
		} else {
			read_len = data_available;
		}

		if(copy_to_user(buf, entry->buffptr + offset, read_len) != 0) {
			// copy failed
			retval = -EFAULT;
			goto read_out;
		}
	} else { // at end of circular buffer
		read_len = 0;
	}

	*f_pos += read_len;
	retval = read_len;

  read_out:	
	mutex_unlock(&aesd_device.lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	size_t total_count;
	size_t start_index;
	// todo - what is f_pos supposed to do here? Possibly used in next assignment?
	
	if(mutex_lock_interruptible(&aesd_device.lock) != 0) {
		// couldn't lock
		return -ERESTARTSYS;
	}

	if(aesd_device.temp_write_data == NULL) {
		// get memory
		total_count = count;
		start_index = 0;
		aesd_device.temp_write_data = kmalloc(total_count, GFP_KERNEL);
	} else {
		// get memory for both new and existing
		size_t previous_count = strlen(aesd_device.temp_write_data); 
		total_count = count + previous_count;
		start_index = previous_count;
		aesd_device.temp_write_data = krealloc(aesd_device.temp_write_data, total_count, GFP_KERNEL);
	}
	
	if(aesd_device.temp_write_data == NULL) {
		// failed to allocate memory
		retval = -ENOMEM;
		goto write_out;
	}

	if(copy_from_user(&aesd_device.temp_write_data[start_index], buf, count) != 0) {
		// copy failed
		retval = -EFAULT;
		goto write_out;
	}

	if(aesd_device.temp_write_data[total_count] == '\n') {
		// data is terminated with newline - push to buffer
		struct aesd_buffer_entry entry = {.size=total_count, .buffptr=aesd_device.temp_write_data};
		char* old_data = aesd_circular_buffer_add_entry(&aesd_device.buffer, &entry);
		kfree(old_data); // can be passed to kfree, even if NULL
		aesd_device.temp_write_data = NULL; // Has been saved to buffer.
	}
	
	retval = count;

  write_out:	
	mutex_unlock(&aesd_device.lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
	mutex_init(&aesd_device.lock);
	
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
	uint8_t index;
	struct aesd_buffer_entry *entry;
	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
		kfree(entry->buffptr);
	}

	kfree(aesd_device.temp_write_data);  // No need to check for NULL, just pass everything in.

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);