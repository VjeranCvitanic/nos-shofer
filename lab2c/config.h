/*
 * config.h -- structures, constants, macros
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#pragma once

#define DRIVER_NAME 	"shofer"

#define AUTHOR		"Leonardo Jelenkovic"
#define LICENSE		"Dual BSD/GPL"

#define DRIVER_NUM	6

#define PIPE_SIZE		64
#define MAX_THREADS		5

struct pipe {
	size_t pipe_size;
	size_t max_threads;
	size_t thread_cnt;

 	struct kfifo fifo;
	struct semaphore cs_readers;	//kritični odsječak za čitače - čitaju jedan po jedan!
	struct semaphore empty;		//ako nema ništa u cijevi čitač koji je u KO čeka
	int reader_waiting;		//čeka li čitač?
	struct semaphore cs_writers;	//kritični odsječak za pisače - pišu jedan po jedan!
	struct semaphore full;		//ako je cijev puna pisač koji je u KO čeka
	int writter_waiting;		//čeka li pisač?
	struct mutex lock;		//za medjusobno iskljucivanje
};

/* Device driver */
struct shofer_dev {
	dev_t dev_no;		/* device number */
	struct pipe *pipe;		/* cijev */
	struct cdev cdev;	/* Char device structure */
	struct list_head list;
	int id;			/* id to differentiate drivers in prints */
};


#define klog(LEVEL, format, ...)	\
printk(LEVEL "[shofer] %d: " format "\n", __LINE__, ##__VA_ARGS__)

//printk(LEVEL "[shofer]%s:%d]" format "\n", __FILE__, __LINE__, ##__VA_ARGS__)
//printk(LEVEL "[shofer]%s:%d]" format "\n", __FILE_NAME__, __LINE__, ##__VA_ARGS__)

//#define SHOFER_DEBUG

#ifdef SHOFER_DEBUG
#define LOG(format, ...)	klog(KERN_DEBUG, format,  ##__VA_ARGS__)
#else /* !SHOFER_DEBUG */
#warning Debug not activated
#define LOG(format, ...)
#endif /* SHOFER_DEBUG */
