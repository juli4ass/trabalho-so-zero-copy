#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Julia e Grupo - FURB");
MODULE_DESCRIPTION("Driver Zero-Copy: read vs mmap com afinidade NUMA");
MODULE_VERSION("2.1");

#define DEVICE_NAME "ezdma"
#define CLASS_NAME  "ezdma_class"
#define BUFFER_SIZE (4 * 1024)

static int            major_number;
static struct class  *ezdma_class  = NULL;
static struct device *ezdma_device = NULL;
static char          *dma_buffer   = NULL;

static char *ezdma_devnode(const struct device *dev, umode_t *mode)
{
    if (mode) *mode = 0666;
    return NULL;
}

static ssize_t dev_read(struct file *filp, char __user *buf,
                        size_t len, loff_t *off)
{
    size_t to_copy;
    if (*off >= BUFFER_SIZE) return 0;
    to_copy = min(len, (size_t)(BUFFER_SIZE - *off));
    if (copy_to_user(buf, dma_buffer + *off, to_copy)) return -EFAULT;
    *off += to_copy;
    return to_copy;
}

static int dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    int ret;
    pfn = page_to_pfn(virt_to_page(dma_buffer));
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    ret = remap_pfn_range(vma, vma->vm_start, pfn,
                          vma->vm_end - vma->vm_start, vma->vm_page_prot);
    if (ret < 0) {
        printk(KERN_ALERT "ezdma: falha remap_pfn_range\n");
        return ret;
    }
    printk(KERN_INFO "ezdma [MMAP]: pagina mapeada\n");
    return 0;
}

static int dev_open(struct inode *inod, struct file *fil)    { return 0; }
static int dev_release(struct inode *inod, struct file *fil) { return 0; }

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .mmap    = dev_mmap,
};

static int __init ezdma_init(void)
{
    printk(KERN_INFO "ezdma: inicializando v2.1\n");
    dma_buffer = kmalloc_node(BUFFER_SIZE, GFP_KERNEL, 0);
    if (!dma_buffer) return -ENOMEM;
    SetPageReserved(virt_to_page(dma_buffer));
    snprintf(dma_buffer, BUFFER_SIZE,
             "DADO EM MEMORIA COMPARTILHADA - NUMA 0 - FURB Zero-Copy\n");
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        ClearPageReserved(virt_to_page(dma_buffer));
        kfree(dma_buffer);
        return major_number;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    ezdma_class = class_create(THIS_MODULE, CLASS_NAME);
#else
    ezdma_class = class_create(CLASS_NAME);
#endif
    if (IS_ERR(ezdma_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        ClearPageReserved(virt_to_page(dma_buffer));
        kfree(dma_buffer);
        return PTR_ERR(ezdma_class);
    }
    ezdma_class->devnode = ezdma_devnode;
    ezdma_device = device_create(ezdma_class, NULL,
                                 MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(ezdma_device)) {
        class_destroy(ezdma_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        ClearPageReserved(virt_to_page(dma_buffer));
        kfree(dma_buffer);
        return PTR_ERR(ezdma_device);
    }
    printk(KERN_INFO "ezdma: /dev/ezdma pronto (read+mmap, NUMA 0)\n");
    return 0;
}

static void __exit ezdma_exit(void)
{
    device_destroy(ezdma_class, MKDEV(major_number, 0));
    class_destroy(ezdma_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    if (dma_buffer) {
        ClearPageReserved(virt_to_page(dma_buffer));
        kfree(dma_buffer);
    }
    printk(KERN_INFO "ezdma: descarregado\n");
}

module_init(ezdma_init);
module_exit(ezdma_exit);
