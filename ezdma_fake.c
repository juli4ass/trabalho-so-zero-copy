/*
 * ezdma_fake.c - v3.0 (Semana 3 - DMA Real)
 *
 * Driver Linux LKM que demonstra Zero-Copy I/O usando a
 * DMA Coherent API REAL do kernel (dma_alloc_coherent +
 * dma_mmap_coherent), com platform_device como struct device*.
 *
 * Mudancas vs v2.1:
 *   - kmalloc_node()        -> dma_alloc_coherent()
 *   - remap_pfn_range()     -> dma_mmap_coherent()
 *   - SetPageReserved()     -> nao precisa (DMA-coherent api gerencia)
 *   - Adiciona platform_device como "struct device*" para a DMA API
 *   - Adiciona dma_addr_t (bus address real)
 *   - Adiciona dma_set_coherent_mask(64) (compatibilidade ampla)
 *
 * Por que isso e DMA REAL:
 *   - dma_alloc_coherent retorna memoria DMA-coherent, com cache
 *     coherency garantido entre CPU e qualquer hardware DMA
 *   - dma_handle (dma_addr_t) e um "bus address" - endereco que
 *     um controlador DMA fisico usaria para acessar a RAM
 *   - dma_mmap_coherent mapeia esse bus address ao user space
 *   - APIs identicas as usadas em drivers de producao (NVMe, iwlwifi)
 *
 * Autor: Julia da Assuncao Silva e grupo - FURB / Sistemas de Informacao
 * Disciplina: Sistemas Operacionais
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>      /* DMA Coherent API */
#include <linux/platform_device.h>  /* platform_device para ter struct device* */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Julia e Grupo - FURB");
MODULE_DESCRIPTION("Driver Zero-Copy v3.0 - DMA REAL via dma_alloc_coherent");
MODULE_VERSION("3.0");

#define DEVICE_NAME "ezdma"
#define CLASS_NAME  "ezdma_class"
#define BUFFER_SIZE (4 * 1024)   /* 4 KB = 1 pagina */

static int                      major_number;
static struct class            *ezdma_class  = NULL;
static struct device           *ezdma_device = NULL;
static struct platform_device  *ezdma_pdev   = NULL;  /* novo: device para DMA API */
static void                    *dma_buffer   = NULL;  /* endereco VIRTUAL no kernel */
static dma_addr_t               dma_handle;            /* novo: BUS ADDRESS real */

/* -----------------------------------------------------------
 * Permissoes do /dev/ezdma: 0666 (qualquer usuario pode ler/escrever)
 * Evita "Permission denied" se rodar benchmark sem sudo
 * ----------------------------------------------------------- */
static char *ezdma_devnode(const struct device *dev, umode_t *mode)
{
    if (mode) *mode = 0666;
    return NULL;
}

/* -----------------------------------------------------------
 * read() tradicional - copy_to_user (caminho baseline)
 * Mantido para comparacao com mmap (Zero-Copy)
 * ----------------------------------------------------------- */
static ssize_t dev_read(struct file *filp, char __user *buf,
                        size_t len, loff_t *off)
{
    size_t to_copy;

    if (*off >= BUFFER_SIZE)
        return 0;

    to_copy = min(len, (size_t)(BUFFER_SIZE - *off));

    if (copy_to_user(buf, dma_buffer + *off, to_copy))
        return -EFAULT;

    *off += to_copy;
    return to_copy;
}

/* -----------------------------------------------------------
 * mmap() Zero-Copy via DMA Coherent API REAL
 *
 * Em v2.1 usavamos remap_pfn_range manualmente.
 * Em v3.0 usamos dma_mmap_coherent que:
 *   1. Mapeia a regiao DMA-coherent diretamente no user space
 *   2. Garante cache coherency entre CPU e device
 *   3. Configura as page tables de forma DMA-aware
 *   4. Esta na MESMA API usada por drivers NVMe/GPU
 * ----------------------------------------------------------- */
static int dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    size_t size = vma->vm_end - vma->vm_start;

    if (size > BUFFER_SIZE) {
        printk(KERN_ALERT "ezdma: mmap requisitou %zu bytes (max %d)\n",
               size, BUFFER_SIZE);
        return -EINVAL;
    }

    /*
     * dma_mmap_coherent: mapeia o buffer DMA-coherent no user space.
     * Argumentos:
     *   &ezdma_pdev->dev  -> struct device* registrado
     *   vma               -> VMA do processo de usuario
     *   dma_buffer        -> endereco virtual no kernel (CPU view)
     *   dma_handle        -> bus address (device view)
     *   size              -> tamanho a mapear
     */
    ret = dma_mmap_coherent(&ezdma_pdev->dev, vma,
                            dma_buffer, dma_handle, size);
    if (ret < 0) {
        printk(KERN_ALERT "ezdma: dma_mmap_coherent falhou (%d)\n", ret);
        return ret;
    }

    printk(KERN_INFO "ezdma [MMAP]: pagina DMA-coherent mapeada "
                     "| virt=%p bus=%pad size=%zu\n",
           dma_buffer, &dma_handle, size);
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

/* -----------------------------------------------------------
 * Inicializacao do modulo
 * Sequencia:
 *   1. Registra platform_device (pra ter struct device*)
 *   2. Configura DMA mask (64-bit)
 *   3. Aloca buffer DMA-coherent (dma_alloc_coherent)
 *   4. Registra character device (/dev/ezdma)
 *   5. Cria classe sysfs + devnode (udev cria /dev/ezdma)
 * ----------------------------------------------------------- */
static int __init ezdma_init(void)
{
    int ret;

    printk(KERN_INFO "ezdma: inicializando v3.0 (DMA Real)\n");

    /* 1. Registra um platform_device dummy.
     * Isso nos da uma struct device* que e necessaria
     * para todas as funcoes da DMA Coherent API. */
    ezdma_pdev = platform_device_register_simple("ezdma", -1, NULL, 0);
    if (IS_ERR(ezdma_pdev)) {
        printk(KERN_ALERT "ezdma: platform_device_register_simple falhou\n");
        return PTR_ERR(ezdma_pdev);
    }

    /* 2. Configura DMA mask para 64 bits.
     * Indica ao subsistema DMA que nosso "dispositivo"
     * suporta acessar qualquer endereco da RAM (ate 2^64). */
    ret = dma_set_coherent_mask(&ezdma_pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        printk(KERN_ALERT "ezdma: dma_set_coherent_mask falhou (%d)\n", ret);
        goto err_pdev;
    }

    /* 3. ALOCACAO DMA-COHERENT.
     * Retorna:
     *   - dma_buffer: endereco virtual no kernel space (CPU view)
     *   - dma_handle: bus address (endereco que um hw DMA usaria)
     * A memoria e cache-coherent: CPU e device veem o MESMO dado
     * sem precisar de invalidacao manual de cache. */
    dma_buffer = dma_alloc_coherent(&ezdma_pdev->dev, BUFFER_SIZE,
                                    &dma_handle, GFP_KERNEL);
    if (!dma_buffer) {
        printk(KERN_ALERT "ezdma: dma_alloc_coherent falhou\n");
        ret = -ENOMEM;
        goto err_pdev;
    }

    printk(KERN_INFO "ezdma: DMA buffer alocado | "
                     "virt=%p bus=%pad size=%d\n",
           dma_buffer, &dma_handle, BUFFER_SIZE);

    /* Escreve dado de demonstracao no buffer */
    snprintf(dma_buffer, BUFFER_SIZE,
             "DADO EM MEMORIA DMA-COHERENT - FURB Zero-Copy v3.0\n");

    /* 4. Registra major number dinamico */
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "ezdma: register_chrdev falhou (%d)\n", major_number);
        ret = major_number;
        goto err_dma;
    }

    /* 5. Cria classe sysfs.
     * Compatibilidade kernel 5.x e 6.x:
     *   - Antes do 6.4: class_create(THIS_MODULE, name)
     *   - A partir do 6.4: class_create(name) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    ezdma_class = class_create(THIS_MODULE, CLASS_NAME);
#else
    ezdma_class = class_create(CLASS_NAME);
#endif
    if (IS_ERR(ezdma_class)) {
        ret = PTR_ERR(ezdma_class);
        printk(KERN_ALERT "ezdma: class_create falhou (%d)\n", ret);
        goto err_chrdev;
    }
    ezdma_class->devnode = ezdma_devnode;

    /* 6. Cria /dev/ezdma via udev */
    ezdma_device = device_create(ezdma_class, NULL,
                                 MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(ezdma_device)) {
        ret = PTR_ERR(ezdma_device);
        printk(KERN_ALERT "ezdma: device_create falhou (%d)\n", ret);
        goto err_class;
    }

    printk(KERN_INFO "ezdma: /dev/%s pronto "
                     "(read + mmap, DMA-coherent real)\n",
           DEVICE_NAME);
    return 0;

/* Tratamento de erros - libera tudo na ordem inversa */
err_class:
    class_destroy(ezdma_class);
err_chrdev:
    unregister_chrdev(major_number, DEVICE_NAME);
err_dma:
    dma_free_coherent(&ezdma_pdev->dev, BUFFER_SIZE, dma_buffer, dma_handle);
err_pdev:
    platform_device_unregister(ezdma_pdev);
    return ret;
}

/* -----------------------------------------------------------
 * Encerramento - libera recursos na ordem INVERSA da alocacao
 * ----------------------------------------------------------- */
static void __exit ezdma_exit(void)
{
    device_destroy(ezdma_class, MKDEV(major_number, 0));
    class_destroy(ezdma_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    /* Libera o buffer DMA-coherent */
    if (dma_buffer) {
        dma_free_coherent(&ezdma_pdev->dev, BUFFER_SIZE,
                          dma_buffer, dma_handle);
    }

    /* Desregistra platform_device */
    if (ezdma_pdev)
        platform_device_unregister(ezdma_pdev);

    printk(KERN_INFO "ezdma: descarregado (v3.0 DMA Real)\n");
}

module_init(ezdma_init);
module_exit(ezdma_exit);
