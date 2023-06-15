# Device-Driver

C - VS code, done in virtual machine

Nov - Dec 2022

- When the module is loaded, the device is created. An empty list of messages is created as well.
- Removing the module deallocates all messages, removes the list of messages and removes the device.
- Reading from the device returns one message, and removes this message from the kernel list. If the list of messages is empty, the reader returns -EAGAIN.
- Writing to the device stores the message in kernel space and adds it to the list if the message is below the maximum size, and the limit of the number of all messages stored in the kernel  wouldn't be surpassed with this message. If the message is too big, -EINVAL is returned, and if the limit of the number of all messages was surpassed, -EBUSY is returned.
- Handle several read and write attempts concurrently
- obtain the messages in a FIFO manner
---
Useful commands:
1. "ls -l /dev/" check the registered device name
2. "sudo insmod charDeviceDriver.ko" insert the module to kernel
3. "sudo rmmod charDeviceDriver.ko" remove the kernel module
4. "dmesg -w &" or "tail -f /var/log/syslog" check the kernel log printed by "printk()"
5. "mknod /dev/chardev c <major> <minor>" make a device to talk to (check syslog for this, do this before read or write to device)
6. "echo <message> > /dev/<devicename>" send message to the device (use "sudo su" change to root user before execute this)
7. "head -n 1 < /dev/<devicename>" read one message from the device(use "sudo su" change to root user before execute this)
