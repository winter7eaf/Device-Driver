/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <linux/slab.h>
#include <linux/sched.h>
#include "charDeviceDriver.h"

#define RESET_SIZE 0

MODULE_LICENSE("GPL");

struct msg_t {
    char *buf;
    size_t size;
    struct msg_t *next;
};


#define MSGSIZE 4*1024 // 4K

#define MAX_NO_OF_MESSAGES 1000

int noOfMessages = 0;
struct msg_t *messages = NULL;
struct msg_t *lastMsg = NULL;



DEFINE_MUTEX(msg_lock); // exclusive access to message lists 


/*
 * This function is called when the module is loaded
 */
int init_module(void)
{

    Major = register_chrdev(0, DEVICE_NAME, &fops);

    if (Major < 0) {
	printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	return Major;
    }

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
    printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
    printk(KERN_INFO "the device file.\n");
    printk(KERN_INFO "Remove the device file and module when done.\n");

    return 0;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    struct msg_t *tmp;

    /*  Unregister the device */
    unregister_chrdev(Major, DEVICE_NAME);

    /* discard all leftover messages */
    while (messages) {
	tmp = messages->next;
	kfree (messages->buf);
	kfree (messages);
	messages = tmp;
    }
}

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
    
    try_module_get(THIS_MODULE);
    return 0;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char __user *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
    struct msg_t *tmp;
    int res;
    
    // extract message at the head 
    mutex_lock(&msg_lock);
    if (messages == NULL) {
        mutex_unlock(&msg_lock);
	printk (KERN_INFO "No messages found\n");
	return -EAGAIN;
    }
    tmp = messages;
    messages = messages->next;
    noOfMessages--;
    if (messages == NULL) {
	lastMsg = NULL;
    }
    mutex_unlock(&msg_lock);

    // now copy message to user space
    if (length < tmp->size) {
	// it would be also OK to copy only length bytes from the message in this case - the behaviour in this case was not specified
	res = -EFAULT;
    }
    else if (copy_to_user (buffer, tmp->buf, tmp->size)) {
	res = -EFAULT;
    }
    else // copy successful
	res = tmp->size;

    // free read message
    kfree (tmp->buf);
    kfree (tmp);
    return res;
}
    
    
/* Called when a process writes to dev file */
static ssize_t
device_write(struct file *filp, const char __user *buff, size_t len, loff_t * off)
{
    char *kernelBuffer;
    struct msg_t *msg;

    if (len > MSGSIZE) {
	return -EINVAL;
    }

    // prepare new list element first 
    kernelBuffer = kmalloc (len, GFP_KERNEL);
    if (!kernelBuffer) {
	return -ENOMEM;
    }

    msg = kmalloc (sizeof (struct msg_t), GFP_KERNEL);
    if (!msg) {
	kfree (kernelBuffer);
	return -ENOMEM;
    }

    // copy data from user space
    msg->buf = kernelBuffer;
    if (copy_from_user (msg->buf, buff, len)) {
	kfree (msg);
	kfree (kernelBuffer);
	return -EFAULT;
    }
    msg->next = NULL;
    msg->size = len;

    // add element to the list 
    mutex_lock(&msg_lock);
    if (noOfMessages == MAX_NO_OF_MESSAGES) {
        mutex_unlock(&msg_lock);
	printk (KERN_INFO "Maximum number of messages reached, not adding messgae\n");
	kfree(msg);
	kfree(kernelBuffer);
	return -EBUSY;
    }
    if (lastMsg == NULL) {
	messages = msg;
    }
    else {
	lastMsg->next = msg;
    }
    lastMsg  = msg;
    noOfMessages++;
    mutex_unlock(&msg_lock);

    return len;

}
