// SPDX-License-Identifier: GPL-2.0
/* Exclusive, non-mappable controller access. Administrative FPGA reload must be
 * prohibited while owned. A wedged AXI fabric is not made recoverable by ioctl.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/compat.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "dmotor_ioctl.h"
#include "dmotor_regs.h"
#define ID DM_IDENTITY
#define STATUS DM_STATUS
#define SESSION DM_SESSION
#define RESET DM_CORE_RESET
#define PROFILE DM_PROFILE
#define BLOCKED BIT(18)
#define HALT_POLL_US 10
#define HALT_WAIT_US 10000
struct dmotor {
 struct miscdevice misc;
 struct gpio_desc *halt;
 void __iomem *base;
 struct mutex lock;
 struct kref refs;
 bool opened, dead;
 struct notifier_block overlay_notifier;
};
static void destroy(struct kref *ref) { kfree(container_of(ref, struct dmotor, refs)); }
static int halt_all(struct dmotor *d)
{
 u32 status;
 gpiod_set_value_cansleep(d->halt, 1);
 if (gpiod_get_value_cansleep(d->halt) != 1) return -EIO;
 return readl_poll_timeout(d->base + STATUS, status,
                          !(status & 15) && (status & BLOCKED), HALT_POLL_US, HALT_WAIT_US);
}
static int dm_open(struct inode *inode, struct file *file)
{
 struct dmotor *d = container_of(file->private_data, struct dmotor, misc);
 int ret = 0;
 mutex_lock(&d->lock);
 if (d->dead) ret = -ENODEV;
 else if (d->opened) ret = -EBUSY;
 else {
  ret = halt_all(d);
  if (!ret) { d->opened = true; kref_get(&d->refs); file->private_data = d; }
 }
 mutex_unlock(&d->lock);
 return ret;
}
static int dm_release(struct inode *inode, struct file *file)
{
 struct dmotor *d = file->private_data;
 mutex_lock(&d->lock);
 if (!d->dead && halt_all(d)) pr_err("dmotor: close could not confirm HALT\n");
 d->opened = false;
 mutex_unlock(&d->lock);
 kref_put(&d->refs, destroy);
 return 0;
}
static bool readable(u32 a)
{
 if (a & 3) return false;
 return a <= 0x40 || (a >= 0x100 && a < 0x140) ||
        a == ID || a == STATUS || a == SESSION || a == DM_SNAPSHOT ||
        (a >= DM_CONFIG_BASE && a < (DM_CONFIG_BASE + 4 * DM_CONFIG_STRIDE) && (a & (DM_CONFIG_STRIDE - 1)) < 80) ||
        (a >= DM_TELEMETRY_BASE && a < (DM_TELEMETRY_BASE + 4 * DM_TELEMETRY_STRIDE));
}
static bool writable(u32 a, u32 v, u32 status)
{
 if (a & 3) return false;
 if ((a >= 0x10 && a <= 0x18) || (a >= 0x80 && a < 0xa0)) return true;
 if (a == 0x1c || a == DM_SNAPSHOT) return v == 1;
 if (a == DM_ENABLE || a == DM_DISABLE || a == DM_STOP) return v < 16;
 if (a == DM_REFRESH_SESSION || (a >= DM_REFRESH_BASE && a < (DM_REFRESH_BASE + 4 * 4))) return v != 0;
 if (status & 15) return false;
 if (a == SESSION) return v != 0;
 if (a == RESET) return v <= 1;
 if (a == DM_ACKNOWLEDGE) return v < 8;
 if (a == DM_WATCHDOG_CYCLES || a == DM_TARGET_TIMEOUT_CYCLES) return v != 0;
 if (a >= DM_CONFIG_BASE && a < (DM_CONFIG_BASE + 4 * DM_CONFIG_STRIDE))
  return (a & (DM_CONFIG_STRIDE - 1)) < 80 || ((a & (DM_CONFIG_STRIDE - 1)) == DM_CONFIG_COMMIT && v == 1);
 return false;
}
static long dm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
 struct dmotor *d = file->private_data;
 struct dmotor_access a;
 u32 status;
 int ret = 0;
 if (cmd != DMOTOR_READ && cmd != DMOTOR_WRITE && cmd != DMOTOR_HALT && cmd != DMOTOR_RELEASE_HALT)
  return -ENOTTY;
 if ((cmd == DMOTOR_READ || cmd == DMOTOR_WRITE) && copy_from_user(&a, (void __user *)arg, sizeof(a))) return -EFAULT;
 mutex_lock(&d->lock);
 if (d->dead) { ret = -ENODEV; goto out; }
 if (cmd == DMOTOR_HALT) { ret = halt_all(d); goto out; }
 status = readl(d->base + STATUS);
 if (cmd == DMOTOR_RELEASE_HALT) {
  if (status & 15) ret = -EBUSY;
  else gpiod_set_value_cansleep(d->halt, 0);
  goto out;
 }
 if (a.offset < 0x1000 && (status & BLOCKED)) { ret = -EAGAIN; goto out; }
 if (cmd == DMOTOR_READ) {
  if (!readable(a.offset)) { ret = -EINVAL; goto out; }
  a.value = readl(d->base + a.offset);
  if (copy_to_user((void __user *)arg, &a, sizeof(a))) ret = -EFAULT;
 } else {
  if (!writable(a.offset, a.value, status)) { ret = -EINVAL; goto out; }
  writel(a.value, d->base + a.offset);
  /* Ordered same-device completion read, not proof that an action was accepted. */
  if (readl(d->base + ID) != PROFILE) ret = -EIO;
 }
out:
 mutex_unlock(&d->lock);
 return ret;
}
static const struct file_operations ops = {
 .owner = THIS_MODULE, .open = dm_open, .release = dm_release,
 .unlocked_ioctl = dm_ioctl, .compat_ioctl = compat_ptr_ioctl,
};
static int overlay_event(struct notifier_block *nb, unsigned long action, void *data)
{
 struct dmotor *d = container_of(nb, struct dmotor, overlay_notifier);
 /* Conservative: no overlay removal while the hardware owner is attached. */
 if (action == OF_OVERLAY_PRE_REMOVE && READ_ONCE(d->opened)) return notifier_from_errno(-EBUSY);
 return NOTIFY_OK;
}
static int dm_probe(struct platform_device *pdev)
{
 struct dmotor *d;
 struct resource *r;
 int ret;
 d = kzalloc(sizeof(*d), GFP_KERNEL);
 if (!d) return -ENOMEM;
 mutex_init(&d->lock); kref_init(&d->refs);
 d->halt = devm_gpiod_get(&pdev->dev, "halt", GPIOD_OUT_HIGH);
 if (IS_ERR(d->halt)) { ret = PTR_ERR(d->halt); goto fail; }
 r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
 if (!r || resource_size(r) < (DM_TELEMETRY_BASE + 4 * DM_TELEMETRY_STRIDE)) { ret = -EINVAL; goto fail; }
 d->base = devm_ioremap_resource(&pdev->dev, r);
 if (IS_ERR(d->base)) { ret = PTR_ERR(d->base); goto fail; }
 if (readl(d->base + ID) != PROFILE) { ret = -ENODEV; goto fail; }
 ret = halt_all(d); if (ret) goto fail;
 d->misc.minor = MISC_DYNAMIC_MINOR; d->misc.name = "dmotor0";
 d->misc.fops = &ops; d->misc.parent = &pdev->dev; d->misc.mode = 0600;
 d->overlay_notifier.notifier_call = overlay_event;
 ret = of_overlay_notifier_register(&d->overlay_notifier); if (ret) goto fail;
 ret = misc_register(&d->misc);
 if (ret) { of_overlay_notifier_unregister(&d->overlay_notifier); goto fail; }
 platform_set_drvdata(pdev, d); return 0;
fail:
 kref_put(&d->refs, destroy); return ret;
}
static void dm_remove(struct platform_device *pdev)
{
 struct dmotor *d = platform_get_drvdata(pdev);
 of_overlay_notifier_unregister(&d->overlay_notifier);
 misc_deregister(&d->misc);
 mutex_lock(&d->lock);
 if (halt_all(d)) dev_err(&pdev->dev, "Cannot confirm HALT on removal\n");
 d->dead = true;
 mutex_unlock(&d->lock);
 kref_put(&d->refs, destroy);
}
static const struct of_device_id matches[] = { { .compatible = "dmotor,controller-0.18" }, {} };
MODULE_DEVICE_TABLE(of, matches);
static struct platform_driver dmotor_platform_driver = {
 .probe = dm_probe, .remove = dm_remove,
 .driver = { .name = "dmotor-access", .of_match_table = matches },
};
module_platform_driver(dmotor_platform_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DMOTOR exclusive access and GPIO HALT foundation");
