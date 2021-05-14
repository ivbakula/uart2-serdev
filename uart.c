#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/serdev.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/kref.h>
#include <linux/fs.h>
#include <linux/slab.h>

#define DEFAULT_BAUDRATE	115200
#define BUFFER_LENGTH		256	

struct ttta_serdev_device {
	struct serdev_device *sdev;
	struct cdev cdev;
	struct device *dev;
	struct class *class;
	struct mutex ser_mutex;
	struct kref kref;

	char *data;
	dev_t devt;
};

static const struct serdev_device_ops ttta_serdev_ops;  

static void ttta_uart_delete(struct kref *kref)
{
	struct ttta_serdev_device *ttta_sdev = 
		container_of(kref, struct ttta_serdev_device, kref);

	device_destroy(ttta_sdev->class, ttta_sdev->devt);
	class_destroy(ttta_sdev->class);
	unregister_chrdev_region(ttta_sdev->devt, 1);
	kfree(ttta_sdev->data);
	kfree(ttta_sdev);
}

static int ttta_uart_open(struct inode *inode, struct file *file) 
{
	struct ttta_serdev_device *dev;
	dev = container_of(inode->i_cdev, struct ttta_serdev_device, cdev);

	if(!dev)
		return -ENODEV;

	file->private_data = dev;
	kref_get(&dev->kref);

	return 0;
}

static int ttta_uart_release(struct inode *inode, struct file *file)
{
	struct ttta_serdev_device *dev;
	dev = file->private_data;
	if (!dev)
		return -ENODEV;

	kref_put(&dev->kref, ttta_uart_delete);
	return 0;
}

static ssize_t ttta_uart_write(struct file *file, const char __user *buff, 
		size_t count, loff_t *ppos) 
{
	struct ttta_serdev_device *dev = file->private_data;
	int retval;

	if (count == 0)
		return 0;

	if (count > BUFFER_LENGTH)
		count = BUFFER_LENGTH;

	/* nonblocking writes must not wait */
	if ((file->f_flags & O_NONBLOCK) && 
			!mutex_trylock(&dev->ser_mutex)) 
		return -EAGAIN;

	retval = mutex_lock_interruptible(&dev->ser_mutex);
	if (retval < 0)
		return retval;

	if (copy_from_user(dev->data, buff, count)) {
		retval = -EFAULT;
		goto error;
	}
	retval = serdev_device_write(dev->sdev, dev->data, count, 
						MAX_SCHEDULE_TIMEOUT);
error:
	mutex_unlock(&dev->ser_mutex);
	return retval;
}

static struct file_operations uart_fops = {
	.open = ttta_uart_open,
	.release = ttta_uart_release,
	.write = ttta_uart_write,
};

static int ttta1234_chrdev_init(struct serdev_device *sdev)
{
	struct ttta_serdev_device *ttta_sdev; 
	int retval;
	ttta_sdev = kzalloc(sizeof(*ttta_sdev), GFP_KERNEL);
	if (!ttta_sdev) 
		return -ENOMEM;		

	ttta_sdev->data = kmalloc_array(BUFFER_LENGTH, sizeof(char), GFP_KERNEL);
	if (!ttta_sdev->data) {
		retval = -ENOMEM;
		goto err_data;
	}

	kref_init(&ttta_sdev->kref);
	mutex_init(&ttta_sdev->ser_mutex);
	ttta_sdev->sdev = sdev;
	serdev_device_set_drvdata(sdev, ttta_sdev);

	/* chardev creation */
	retval = alloc_chrdev_region(&ttta_sdev->devt, 0, 1, "serdev");
	if (retval <0) 
		goto err_region;

	ttta_sdev->class = class_create(THIS_MODULE, "serdev");
	if (IS_ERR(ttta_sdev->class)) {
		retval = PTR_ERR(ttta_sdev->class);
		goto err_class;
	}
	
	ttta_sdev->dev = device_create(ttta_sdev->class, NULL, ttta_sdev->devt, NULL, "serdev");
	if (IS_ERR(ttta_sdev->dev)) {
		retval = PTR_ERR(ttta_sdev->dev);
		goto err_dev;
	}

	cdev_init(&ttta_sdev->cdev, &uart_fops);
	retval = cdev_add(&ttta_sdev->cdev, ttta_sdev->devt, 1);
	if (retval < 0)
		goto err_cdev;

	return 0;	

err_cdev:
	device_destroy(ttta_sdev->class, ttta_sdev->devt);
err_dev:
	class_destroy(ttta_sdev->class);
err_class:
	unregister_chrdev_region(ttta_sdev->devt, 1);
err_region:
	kfree(ttta_sdev->data);
err_data:
	kfree(ttta_sdev);

	return retval;
}

static int ttta1234_uart_probe(struct serdev_device *sdev)
{
	int ret = 0;
	ret = serdev_device_open(sdev);
	if (ret) {
		dev_err(&sdev->dev, "Unable to open device\n");
		return ret;
	}

	ret = ttta1234_chrdev_init(sdev);
	if (ret) {
		dev_err(&sdev->dev, "chrdev_init failed\n");
		serdev_device_close(sdev);
		return ret;
	}
	ret = serdev_device_set_baudrate(sdev, DEFAULT_BAUDRATE);
	if (ret != DEFAULT_BAUDRATE) {
		dev_err(&sdev->dev, "Unable to set baudrate\n");
		serdev_device_close(sdev);
		return -EINVAL;
	}

	serdev_device_set_flow_control(sdev, false);
	serdev_device_set_client_ops(sdev, &ttta_serdev_ops);
	return 0;
}

static void ttta1234_uart_remove(struct serdev_device *sdev)
{
	struct ttta_serdev_device *ttta_sdev;

	ttta_sdev = serdev_device_get_drvdata(sdev);
	serdev_device_set_drvdata(sdev, NULL);

	kref_put(&ttta_sdev->kref, ttta_uart_delete);
}

static int ttta_recive_buf(struct serdev_device *sdev, const unsigned char *data,
							size_t count)
{
	return 0;
}

static const struct serdev_device_ops ttta_serdev_ops = {
	.receive_buf = ttta_recive_buf,
	.write_wakeup = serdev_device_write_wakeup,
};

static const struct of_device_id ttta_uart_of_match[] = {
	{ .compatible = "acme,ttta1234" },
	{ },
};
MODULE_DEVICE_TABLE(of, ttta_uart_of_match);

static struct serdev_device_driver ttta1234_uart_driver = {
	.probe = ttta1234_uart_probe,
	.remove = ttta1234_uart_remove,
	.driver = {
		.name = "ttta1234_uart",
		.of_match_table = of_match_ptr(ttta_uart_of_match),
	},
};
module_serdev_device_driver(ttta1234_uart_driver);
MODULE_LICENSE("GPL");
