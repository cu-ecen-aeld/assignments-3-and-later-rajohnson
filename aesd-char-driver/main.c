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
#include "aesd_ioctl.h"
#include <iso646.h>

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Rob Johnson");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev;

    PDEBUG("open");
	
	// save device information
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
	size_t offset;	
	struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &offset); 
	size_t read_len;
    
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	
	if(mutex_lock_interruptible(&aesd_device.lock) != 0) {
		// couldn't lock
		return -ERESTARTSYS;
	}

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
	size_t total_count;
	size_t start_index;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	// todo - what is f_pos supposed to do here? Possibly used in next assignment?
	
	if(mutex_lock_interruptible(&aesd_device.lock) != 0) {
		// couldn't lock
		return -ERESTARTSYS;
	}

	if(aesd_device.temp_write_data == NULL) {
		// get memory
		total_count = count;
		start_index = 0;
		aesd_device.temp_write_data = kmalloc(count+1, GFP_KERNEL);
		memset(aesd_device.temp_write_data, 0, count+1);
		PDEBUG("kmalloc #%zu", count+1);
	} else {
		// get memory for both new and existing
		size_t previous_count = strlen(aesd_device.temp_write_data); 
		total_count = count + previous_count;
		start_index = previous_count;
		aesd_device.temp_write_data = krealloc(aesd_device.temp_write_data, total_count+1, GFP_KERNEL);
		memset(&aesd_device.temp_write_data[start_index], 0, count+1);
		PDEBUG("krealloc #%zu, old #%zu s: %s", count, previous_count, aesd_device.temp_write_data);
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

	if(aesd_device.temp_write_data[total_count-1] == '\n') {
		// data is terminated with newline - push to buffer
		struct aesd_buffer_entry entry = {.size=total_count, .buffptr=aesd_device.temp_write_data};
		const char* old_data = aesd_circular_buffer_add_entry(&aesd_device.buffer, &entry);
		PDEBUG("Writing %zu bytes to buffer: %s", total_count, aesd_device.temp_write_data);
		kfree(old_data); // can be passed to kfree, even if NULL
		aesd_device.temp_write_data = NULL; // Has been saved to buffer.
	}
	
	retval = count;
	*f_pos += count;

  write_out:	
	mutex_unlock(&aesd_device.lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t newpos = 0;
	size_t buffer_length = 0;
	uint8_t index;
	struct aesd_buffer_entry *entry;
    PDEBUG("aesd_llseek");

	// Take mutex
	if(mutex_lock_interruptible(&aesd_device.lock) != 0) {
		// couldn't lock
		return -ERESTARTSYS;
	}

	// Calculate buffer length
	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
		buffer_length += entry->size;
	}


	// Delegate work to find location to helper function as suggested in assignment video (~8:30)
	newpos = fixed_size_llseek(filp, off, whence, buffer_length);

	if(newpos > 0) {
		filp->f_pos = newpos;
	}

	// Release mutex
	mutex_unlock(&aesd_device.lock);

    return newpos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aesd_seekto seekto;	
	int retval = 0;
	int prev_cmd_offset = 0;
	int i;

    PDEBUG("aesd_ioctl cmd=%i arg=%ld", cmd, arg);

	if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

	switch(cmd) {
		case AESDCHAR_IOCSEEKTO:
		PDEBUG("AESDCHAR_IOCSEEKTO");
		if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0) {
			PDEBUG("Failed to copy arg from userspace.");
			retval = -EFAULT;
			break;
		}
		// Take mutex
		if(mutex_lock_interruptible(&aesd_device.lock) != 0) {
			// couldn't lock
			PDEBUG("couldn't take mutex");
			retval = -ERESTARTSYS;
			break;
		}

		if((seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) or (aesd_device.buffer.entry[seekto.write_cmd].size < seekto.write_cmd_offset)) {
			PDEBUG("bad argument");
			retval = -EINVAL;
			mutex_unlock(&aesd_device.lock);
			break;
		}

		// Count bytes in previous entries 
		for(i = 0; i < seekto.write_cmd; ++i) {
			prev_cmd_offset += aesd_device.buffer.entry[i].size;
		}

		PDEBUG("previous f_pos=%lli", filp->f_pos);
		// Update f_pos
		filp->f_pos = prev_cmd_offset + seekto.write_cmd_offset;
		PDEBUG("new f_pos=%lli", filp->f_pos);
		

		// Release mutex
		mutex_unlock(&aesd_device.lock);
		break;

		default:
		return -ENOTTY;
	}

	return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
	.llseek = 	aesd_llseek,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
	.unlocked_ioctl = aesd_ioctl,
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
	uint8_t index;
	struct aesd_buffer_entry *entry;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
		kfree(entry->buffptr);
	}

	kfree(aesd_device.temp_write_data);  // No need to check for NULL, just pass everything in.

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
