/*
 * shofer.c -- module implementation
 *
 * Example module with devices, lists, klog, delay
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#include <linux/log2.h>

#include "config.h"

static int pipe_size = PIPE_SIZE;
static int max_threads = MAX_THREADS;

module_param(pipe_size, int, S_IRUGO);
MODULE_PARM_DESC(pipe_size, "Pipe size");
module_param(max_threads, int, S_IRUGO);
MODULE_PARM_DESC(max_threads, "Maximal number of threads simultaneously using message queue");

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

static LIST_HEAD(shofers_list); /* A list of devices */

static dev_t Dev_no = 0;

/* prototypes */
static struct shofer_dev *shofer_create(dev_t, struct file_operations *,
	struct pipe *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);
static void dump_buffer(char *, struct shofer_dev *, struct pipe *);
static void simulate_delay(long delay_ms);

static int shofer_open(struct inode *, struct file *);
static int shofer_release(struct inode *inode, struct file *filp);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);
int pipe_init(struct pipe *pipe, size_t pipe_size, size_t max_threads);
static void pipe_delete(struct pipe *pipe);

static struct file_operations shofer_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open,
	.read =     shofer_read,
	.write =    shofer_write,
	.release = 	shofer_release
};

/* init module */
static int __init shofer_module_init(void)
{
	int retval;
	struct shofer_dev *shofer;
	dev_t dev_no = 0;

	klog(KERN_NOTICE, "Module started initialization");

	/* get device number(s) */
	retval = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
	if (retval < 0) {
		klog(KERN_WARNING, "Can't get major device number");
		return retval;
	}
	Dev_no = dev_no; //remember first

	/* Create and add devices to the list */
	shofer = shofer_create(dev_no, &shofer_fops, NULL, &retval);
	if (!shofer)
		goto no_driver;
	list_add_tail(&shofer->list, &shofers_list);
	dev_no = MKDEV(MAJOR(dev_no), MINOR(dev_no) + 1);

	// initialize the pipe 
	if (pipe_init(&shofer->pipe, pipe_size, max_threads)) {
		kfree(shofer);
		klog(KERN_ERR, "Cant init pipe");
		return -1;
	}

	klog(KERN_NOTICE, "Module initialized with major=%d", MAJOR(dev_no));

	return 0;

no_driver:
	cleanup();

	return retval;
}

static void cleanup(void)
{
	struct shofer_dev *shofer, *s;

	list_for_each_entry_safe (shofer, s, &shofers_list, list) {
		list_del (&shofer->list);
		shofer_delete(shofer);
	}

	if (Dev_no)
		unregister_chrdev_region(Dev_no, 1);
}

/* called when module exit */
static void __exit shofer_module_exit(void)
{
	klog(KERN_NOTICE, "Module started exit operation");
	cleanup();
	klog(KERN_NOTICE, "Module finished exit operation");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

/* Create and initialize a single shofer_dev */
static struct shofer_dev *shofer_create(dev_t dev_no,
	struct file_operations *fops, struct pipe *pipe, int *retval)
{
	static int shofer_id = 0;
	struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
	if (!shofer){
		*retval = -ENOMEM;
		klog(KERN_WARNING, "kmalloc failed");
		return NULL;
	}
	memset(shofer, 0, sizeof(struct shofer_dev));
	shofer->pipe = pipe;

	cdev_init(&shofer->cdev, fops);
	shofer->cdev.owner = THIS_MODULE;
	shofer->cdev.ops = fops;
	*retval = cdev_add (&shofer->cdev, dev_no, 1);
	shofer->dev_no = dev_no;
	shofer->id = shofer_id++;
	if (*retval) {
		klog(KERN_WARNING, "Error (%d) when adding device", *retval);
		kfree(shofer);
		shofer = NULL;
	}

	return shofer;
}
static void shofer_delete(struct shofer_dev *shofer)
{
	cdev_del(&shofer->cdev);
	kfree(shofer);
}

static int shofer_open(struct inode *inode, struct file *filp)
{
	struct shofer_dev *shofer;
    shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);

	if (shofer->pipe.thread_cnt >= shofer->pipe.max_threads)
		return -EBUSY;

	shofer->pipe.thread_cnt++;

	filp->private_data = shofer;

	return 0;
}

static void dump_buffer(char *prefix, struct shofer_dev *shofer, struct pipe *b)
{
	char buf[PIPE_SIZE];
	size_t copied;

	memset(buf, 0, PIPE_SIZE);
	copied = kfifo_out_peek(&b->fifo, buf, PIPE_SIZE);

	LOG("%s:id=%d,pipe:size=%u:contains=%u:buf=%s",
	prefix, shofer->id, kfifo_size(&b->fifo), kfifo_len(&b->fifo), buf);
}

static void simulate_delay(long delay_ms)
{
	int retval;
	wait_queue_head_t wait;
	init_waitqueue_head (&wait);
	retval = wait_event_interruptible_timeout(wait, 0,
		msecs_to_jiffies(delay_ms));
}

int pipe_init(struct pipe *pipe, size_t pipe_size, size_t max_threads)
{
	int ret;
	ret = kfifo_alloc(&pipe->fifo, pipe_size, GFP_KERNEL);
	if (ret) {
		klog(KERN_NOTICE, "kfifo_alloc failed");
		return ret;
	}
	pipe->pipe_size = pipe_size;
	pipe->max_threads = max_threads;
	pipe->thread_cnt = 0;

	sema_init(&pipe->cs_readers, 1);
	sema_init(&pipe->cs_writers, 1);
	sema_init(&pipe->empty, 0);
	sema_init(&pipe->full, 1);
	pipe->reader_waiting = 0;
	pipe->writter_waiting = 0;

	mutex_init(&pipe->lock);

	return 0;
}


static void pipe_delete(struct pipe *pipe)
{
	//todo - obrisi sve poruke ako ih ima
	kfifo_free(&pipe->fifo);
}
 
 
/* Called when a process performs "close" operation */
static int shofer_release(struct inode *inode, struct file *filp)
{
	struct shofer_dev *shofer = filp->private_data;
	shofer->pipe.thread_cnt--;

	if(shofer->pipe.thread_cnt <= 0)
		pipe_delete(&shofer->pipe);

	return 0;
}

/* Pročitaj nešto iz cijevi */
static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
 	loff_t *f_pos /* ignoring f_pos */)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct pipe *pipe = &shofer->pipe;
	struct kfifo *fifo = &pipe->fifo;
	unsigned int copied;
 
	if (!( (filp->f_flags & O_ACCMODE) & O_RDONLY))
	{
		LOG("Wrong mode on pipe");
		return -EPERM;
	}

	if (down_interruptible(&pipe->cs_readers))
		return -ERESTARTSYS; //čekanje prekinuto signalom

	//uđi u KO (za cijev)
	while(1) {
		if (mutex_lock_interruptible(&pipe->lock)) {
			//čekanje na ulaz u KO prekinuto signalom
			up(&pipe->cs_readers); //pusti idućeg čitača
			return -ERESTARTSYS;
		}
		if (kfifo_is_empty(fifo)) {
			pipe->reader_waiting = 1;
			mutex_unlock(&pipe->lock); //privremeno izađi iz KO za cijev
			if (down_interruptible(&pipe->empty)) { //čekaj da se nešto stavi
				up(&pipe->cs_readers);
				return -ERESTARTSYS;
			}
		}
		else break;
	}

	// TODO - jel triba citat tu negdi
	dump_buffer("read-start", shofer, pipe);

	retval = kfifo_to_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		klog(KERN_WARNING, "kfifo_to_user failed");
	else
		retval = copied;
	LOG("Read %ld bytes\n", retval);

	simulate_delay(1000);

	simulate_delay(1000);

	dump_buffer("read-end", shofer, pipe);

	pipe->reader_waiting = 0;

	if (pipe->writter_waiting)
		up(&pipe->full); //ako neki pisač čeka, neka sad proba opet

	mutex_unlock(&pipe->lock); //izađi iz KO

	up(&pipe->cs_readers);
	return retval;
}
 
/* Stavi poruku u red */
static ssize_t shofer_write(struct file *filp, const char __user *ubuf,
 	size_t count, loff_t *f_pos /* ignoring f_pos */)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct pipe *pipe = &shofer->pipe;
	struct kfifo *fifo = &pipe->fifo;
	unsigned int copied;
 
	if (!((filp->f_flags & O_ACCMODE) & O_WRONLY))
	{
		LOG("Wrong mode on pipe");
		return -EPERM;
	}

	if (count > pipe->pipe_size)
		return -EFBIG;

	if (down_interruptible(&pipe->cs_writers))
 		return -ERESTARTSYS;
 
	//uđi u KO (za cijev)
	while(1) {
		if (mutex_lock_interruptible(&pipe->lock)) {
			//čekanje na ulaz u KO prekinuto signalom
			up(&pipe->cs_writers); //pusti idućeg pisača
			return -ERESTARTSYS;
		}
		if (kfifo_avail(fifo) < count) {
			pipe->writter_waiting = 1;
			mutex_unlock(&pipe->lock); //privremeno izađi iz KO za cijev
			if (down_interruptible(&pipe->full)) { //čekaj da se nešto uzme
				up(&pipe->cs_writers); //pusti idućeg pisača
				return -ERESTARTSYS;
			}
		}
		else break;
    }
	// TODO - jel triba pisat
	dump_buffer("write-start", shofer, pipe);

	retval = kfifo_from_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		klog(KERN_WARNING, "kfifo_from_user failed");
	else
		retval = copied;
	LOG("Wrote %ld bytes\n", retval);

	simulate_delay(1000);

	simulate_delay(1000);

	dump_buffer("write-end", shofer, pipe);

	pipe->writter_waiting = 0;
 
	if (pipe->reader_waiting)
		up(&pipe->empty); //ako neki čitač čeka, neka sad proba opet

	mutex_unlock(&pipe->lock); //izađi iz KO

	up(&pipe->cs_writers);
 
 	return retval;
}