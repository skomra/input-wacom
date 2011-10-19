/*
 * drivers/input/tablet/wacom_sys.c
 *
 *  USB Wacom tablet support - system specific code
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "wacom_wac.h"
#include "wacom.h"

/* defines to get HID report descriptor */
#define HID_DEVICET_HID		(USB_TYPE_CLASS | 0x01)
#define HID_DEVICET_REPORT	(USB_TYPE_CLASS | 0x02)
#define HID_USAGE_UNDEFINED		0x00
#define HID_USAGE_PAGE			0x05
#define HID_USAGE_PAGE_DIGITIZER	0x0d
#define HID_USAGE_PAGE_DESKTOP		0x01
#define HID_USAGE			0x09
#define HID_USAGE_X			0x30
#define HID_USAGE_Y			0x31
#define HID_USAGE_X_TILT		0x3d
#define HID_USAGE_Y_TILT		0x3e
#define HID_USAGE_FINGER		0x22
#define HID_USAGE_STYLUS		0x20
#define HID_COLLECTION			0xc0

enum {
	WCM_UNDEFINED = 0,
	WCM_DESKTOP,
	WCM_DIGITIZER,
};

struct hid_descriptor {
	struct usb_descriptor_header header;
	__le16   bcdHID;
	u8       bCountryCode;
	u8       bNumDescriptors;
	u8       bDescriptorType;
	__le16   wDescriptorLength;
} __attribute__ ((packed));

/* defines to get/set USB message */
#define USB_REQ_GET_REPORT	0x01
#define USB_REQ_SET_REPORT	0x09
#define WAC_HID_FEATURE_REPORT	0x03

#define WAC_CMD_LED_CONTROL    0x20

static int usb_get_report(struct usb_interface *intf, unsigned char type,
				unsigned char id, void *buf, int size)
{
	return usb_control_msg(interface_to_usbdev(intf),
		usb_rcvctrlpipe(interface_to_usbdev(intf), 0),
		USB_REQ_GET_REPORT, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(type << 8) + id, intf->altsetting[0].desc.bInterfaceNumber,
		buf, size, 100);
}

static int usb_set_report(struct usb_interface *intf, unsigned char type,
				unsigned char id, void *buf, int size)
{
	return usb_control_msg(interface_to_usbdev(intf),
		usb_sndctrlpipe(interface_to_usbdev(intf), 0),
                USB_REQ_SET_REPORT, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                (type << 8) + id, intf->altsetting[0].desc.bInterfaceNumber,
		buf, size, 1000);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static void wacom_sys_irq(struct urb *urb, struct pt_regs *regs)
#else
static void wacom_sys_irq(struct urb *urb)
#endif
{
	struct wacom *wacom = urb->context;
	int retval;

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d", __func__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d", __func__, urb->status);
		goto exit;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
	wacom->wacom_wac.regs = regs;
#endif
	wacom_wac_irq(&wacom->wacom_wac, urb->actual_length);

 exit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		err ("%s - usb_submit_urb failed with result %d",
		     __func__, retval);
}

static int wacom_open(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;

	mutex_lock(&wacom->lock);

	wacom->irq->dev = wacom->usbdev;

	if (usb_submit_urb(wacom->irq, GFP_KERNEL)) {
		mutex_unlock(&wacom->lock);
		return -EIO;
	}

	wacom->open = 1;

	mutex_unlock(&wacom->lock);
	return 0;
}

static void wacom_close(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;

	mutex_lock(&wacom->lock);
	usb_kill_urb(wacom->irq);
	wacom->open = 0;
	mutex_unlock(&wacom->lock);
}

static int wacom_parse_hid(struct usb_interface *intf, struct hid_descriptor *hid_desc,
			   struct wacom_features *features)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	char limit = 0;
	/* result has to be defined as int for some devices */
	int result = 0;
	int i = 0, usage = WCM_UNDEFINED, finger = 0, pen = 0;
	unsigned char *report;

	report = kzalloc(hid_desc->wDescriptorLength, GFP_KERNEL);
	if (!report)
		return -ENOMEM;

	/* retrive report descriptors */
	do {
		result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR,
			USB_RECIP_INTERFACE | USB_DIR_IN,
			HID_DEVICET_REPORT << 8,
			intf->altsetting[0].desc.bInterfaceNumber, /* interface */
			report,
			hid_desc->wDescriptorLength,
			5000); /* 5 secs */
	} while (result < 0 && limit++ < 5);

	/* No need to parse the Descriptor. It isn't an error though */
	if (result < 0)
		goto out;

	for (i = 0; i < hid_desc->wDescriptorLength; i++) {

		switch (report[i]) {
		case HID_USAGE_PAGE:
			switch (report[i + 1]) {
			case HID_USAGE_PAGE_DIGITIZER:
				usage = WCM_DIGITIZER;
				i++;
				break;

			case HID_USAGE_PAGE_DESKTOP:
				usage = WCM_DESKTOP;
				i++;
				break;
			}
			break;

		case HID_USAGE:
			switch (report[i + 1]) {
			case HID_USAGE_X:
				if (usage == WCM_DESKTOP) {
					if (finger) {
						features->device_type = BTN_TOOL_DOUBLETAP;
						if (features->type == TABLETPC2FG) {
							/* need to reset back */
							features->pktlen = WACOM_PKGLEN_TPC2FG;
							features->device_type = BTN_TOOL_TRIPLETAP;
						}
						if (features->type == BAMBOO_PT) {
							/* need to reset back */
							features->pktlen = WACOM_PKGLEN_BBTOUCH;
							features->device_type = BTN_TOOL_TRIPLETAP;
							features->x_phy =
								get_unaligned_le16(&report[i + 5]);
							features->x_max =
								get_unaligned_le16(&report[i + 8]);
							i += 15;
						} else {
							features->x_max =
								get_unaligned_le16(&report[i + 3]);
							features->x_phy =
								get_unaligned_le16(&report[i + 6]);
							features->unit = report[i + 9];
							features->unitExpo = report[i + 11];
							i += 12;
						}
					} else if (pen) {
						/* penabled only accepts exact bytes of data */
						if (features->type == TABLETPC2FG)
							features->pktlen = WACOM_PKGLEN_GRAPHIRE;
						if (features->type == BAMBOO_PT)
							features->pktlen = WACOM_PKGLEN_BBFUN;
						features->device_type = BTN_TOOL_PEN;
						features->x_max =
							get_unaligned_le16(&report[i + 3]);
						i += 4;
					}
				} else if (usage == WCM_DIGITIZER) {
					/* max pressure isn't reported
					features->pressure_max = (unsigned short)
							(report[i+4] << 8  | report[i + 3]);
					*/
					features->pressure_max = 255;
					i += 4;
				}
				break;

			case HID_USAGE_Y:
				if (usage == WCM_DESKTOP) {
					if (finger) {
						features->device_type = BTN_TOOL_DOUBLETAP;
						if (features->type == TABLETPC2FG) {
							/* need to reset back */
							features->pktlen = WACOM_PKGLEN_TPC2FG;
							features->device_type = BTN_TOOL_TRIPLETAP;
							features->y_max =
								get_unaligned_le16(&report[i + 3]);
							features->y_phy =
								get_unaligned_le16(&report[i + 6]);
							i += 7;
						} else if (features->type == BAMBOO_PT) {
							/* need to reset back */
							features->pktlen = WACOM_PKGLEN_BBTOUCH;
							features->device_type = BTN_TOOL_TRIPLETAP;
							features->y_phy =
								get_unaligned_le16(&report[i + 3]);
							features->y_max =
								get_unaligned_le16(&report[i + 6]);
							i += 12;
						} else {
							features->y_max =
								features->x_max;
							features->y_phy =
								get_unaligned_le16(&report[i + 3]);
							i += 4;
						}
					} else if (pen) {
						/* penabled only accepts exact bytes of data */
						if (features->type == TABLETPC2FG)
							features->pktlen = WACOM_PKGLEN_GRAPHIRE;
						if (features->type == BAMBOO_PT)
							features->pktlen = WACOM_PKGLEN_BBFUN;
						features->device_type = BTN_TOOL_PEN;
						features->y_max =
							get_unaligned_le16(&report[i + 3]);
						i += 4;
					}
				}
				break;

			case HID_USAGE_FINGER:
				finger = 1;
				i++;
				break;

			case HID_USAGE_STYLUS:
				pen = 1;
				i++;
				break;

			case HID_USAGE_UNDEFINED:
				if (usage == WCM_DESKTOP && finger) /* capacity */
					features->pressure_max =
						get_unaligned_le16(&report[i + 3]);
				i += 4;
				break;
			}
			break;

		case HID_COLLECTION:
			/* reset UsagePage and Finger */
			finger = usage = 0;
			break;
		}
	}

 out:
	result = 0;
	kfree(report);
	return result;
}

static int wacom_query_tablet_data(struct usb_interface *intf, struct wacom_features *features)
{
	unsigned char *rep_data;
	int limit = 0, report_id = 2;
	int error = -ENOMEM;

	rep_data = kmalloc(2, GFP_KERNEL);
	if (!rep_data)
		return error;

	/* ask to report tablet data if it is 2FGT Tablet PC
	 * OR not a Tablet PC */
	if (features->device_type == BTN_TOOL_TRIPLETAP &&
			(features->type == TABLETPC2FG)) {
		do {
			rep_data[0] = 3;
			rep_data[1] = 4;
			report_id = 3;
			error = usb_set_report(intf, WAC_HID_FEATURE_REPORT,
				report_id, rep_data, 2);
			if (error >= 0)
				error = usb_get_report(intf,
					WAC_HID_FEATURE_REPORT, report_id,
					rep_data, 3);
		} while ((error < 0 || rep_data[1] != 4) && limit++ < 5);
	} else if (features->type != TABLETPC && features->type != TABLETPC2FG) {
		do {
			rep_data[0] = 2;
			rep_data[1] = 2;
			error = usb_set_report(intf, WAC_HID_FEATURE_REPORT,
				report_id, rep_data, 2);
			if (error >= 0)
				error = usb_get_report(intf,
					WAC_HID_FEATURE_REPORT, report_id,
					rep_data, 2);
		} while ((error < 0 || rep_data[1] != 2) && limit++ < 5);
	}

	kfree(rep_data);

	return error < 0 ? error : 0;
}

static int wacom_retrieve_hid_descriptor(struct usb_interface *intf,
		struct wacom_features *features)
{
	int error = 0;
	struct usb_host_interface *interface = intf->cur_altsetting;
	struct hid_descriptor *hid_desc;

	/* default device to penabled */
	features->device_type = BTN_TOOL_PEN;

	/* only Tablet PCs and 2FGT devices need to retrieve the info */
	if ((features->type != TABLETPC) && (features->type != TABLETPC2FG)
			&& (features->type != BAMBOO_PT))
		goto out;

	if (usb_get_extra_descriptor(interface, HID_DEVICET_HID, &hid_desc)) {
		if (usb_get_extra_descriptor(&interface->endpoint[0],
				HID_DEVICET_REPORT, &hid_desc)) {
			printk("wacom: can not retrieve extra class descriptor\n");
			error = 1;
			goto out;
		}
	}
	error = wacom_parse_hid(intf, hid_desc, features);
	if (error)
		goto out;

	/* touch device found but size is not defined. use default */
	if (features->device_type == BTN_TOOL_DOUBLETAP && !features->x_max) {
		features->x_max = 1023;
		features->y_max = 1023;
	}

 out:
	return error;
}

static int wacom_led_control(struct wacom *wacom)
{
	unsigned char *buf;
	int retval, led = wacom->led.select[0] | 0x4;

	buf = kzalloc(9, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (wacom->wacom_wac.features.type == WACOM_21UX2)
		led |= (wacom->led.select[1] << 4) | 0x40;

	buf[0] = WAC_CMD_LED_CONTROL;
	buf[1] = led;
 	buf[2] = wacom->led.llv;
 	buf[3] = wacom->led.hlv;
 	buf[4] = wacom->led.img_lum;

	retval = usb_set_report(wacom->intf, WAC_HID_FEATURE_REPORT,
				  WAC_CMD_LED_CONTROL, buf, 9);
	kfree(buf);

	return retval;
}

static ssize_t wacom_led_select_store(struct device *dev, int set_id,
				      const char *buf, size_t count)
{
	struct wacom *wacom = dev_get_drvdata(dev);
	u8 id = 0;
	int i, err;

	for (i = 0; i < count; i++)
		id = id * 10 + buf[i] - '0';

	mutex_lock(&wacom->lock);

	wacom->led.select[set_id] = id & 0x3;
	err = wacom_led_control(wacom);

	mutex_unlock(&wacom->lock);

	return err < 0 ? err : count;
}

#define DEVICE_LED_SELECT_ATTR(SET_ID)					\
static ssize_t wacom_led##SET_ID##_select_store(struct device *dev,	\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	return wacom_led_select_store(dev, SET_ID, buf, count);		\
}									\
static ssize_t wacom_led##SET_ID##_select_show(struct device *dev,	\
	struct device_attribute *attr, char *buf)			\
{									\
	struct wacom *wacom = dev_get_drvdata(dev);			\
	return snprintf(buf, 2, "%d\n", wacom->led.select[SET_ID]);	\
}									\
static DEVICE_ATTR(status_led##SET_ID##_select, S_IWUSR | S_IRUSR,	\
		   wacom_led##SET_ID##_select_show,			\
		   wacom_led##SET_ID##_select_store)

DEVICE_LED_SELECT_ATTR(0);
DEVICE_LED_SELECT_ATTR(1);

static struct attribute *cintiq_led_attrs[] = {
	&dev_attr_status_led0_select.attr,
	&dev_attr_status_led1_select.attr,
	NULL
};

static struct attribute_group cintiq_led_attr_group = {
	.name = "wacom_led",
	.attrs = cintiq_led_attrs,
};

static struct attribute *intuos4_led_attrs[] = {
	&dev_attr_status_led0_select.attr,
	NULL
};

static struct attribute_group intuos4_led_attr_group = {
	.name = "wacom_led",
	.attrs = intuos4_led_attrs,
};

static int wacom_initialize_leds(struct wacom *wacom)
{
	int error;

	/* Initialize default values */
	switch (wacom->wacom_wac.features.type) {
	case INTUOS4:
	case INTUOS4L:
		wacom->led.select[0] = 0;
 		wacom->led.select[1] = 0;
		wacom->led.llv = 10;
 		wacom->led.hlv = 20;
 		wacom->led.img_lum = 10;
		error = sysfs_create_group(&wacom->intf->dev.kobj,
				   &intuos4_led_attr_group);
		break;

	case WACOM_21UX2:
		wacom->led.select[0] = 0;
		wacom->led.select[1] = 0;
		wacom->led.llv = 0;
 		wacom->led.hlv = 0;
 		wacom->led.img_lum = 0;
		error = sysfs_create_group(&wacom->intf->dev.kobj,
				   &cintiq_led_attr_group);
		break;

	default:
		return 0;
	}

	if (error) {
		dev_err(&wacom->intf->dev,
			"cannot create sysfs group err: %d\n", error);
		return error;
	}

	wacom_led_control(wacom);
	return 0;
}

static void wacom_destroy_leds(struct wacom *wacom)
{
	switch (wacom->wacom_wac.features.type) {
	case INTUOS4:
	case INTUOS4L:
		sysfs_remove_group(&wacom->intf->dev.kobj,
				   &intuos4_led_attr_group);
		break;

	case WACOM_21UX2:
		sysfs_remove_group(&wacom->intf->dev.kobj,
				   &cintiq_led_attr_group);
		break;
	}
}

static int wacom_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;
	struct wacom *wacom;
	struct wacom_wac *wacom_wac;
	struct wacom_features *features;
	struct input_dev *input_dev;
	int error;

	if (!id->driver_info)
		return -EINVAL;

	wacom = kzalloc(sizeof(struct wacom), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!wacom || !input_dev) {
		error = -ENOMEM;
		goto fail1;
	}

	wacom_wac = &wacom->wacom_wac;
	wacom_wac->features = *((struct wacom_features *)id->driver_info);
	features = &wacom_wac->features;
	if (features->pktlen > WACOM_PKGLEN_MAX) {
		error = -EINVAL;
		goto fail1;
	}

	wacom_wac->data = usb_buffer_alloc(dev, WACOM_PKGLEN_MAX,
					   GFP_KERNEL, &wacom->data_dma);
	if (!wacom_wac->data) {
		error = -ENOMEM;
		goto fail1;
	}

	wacom->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!wacom->irq) {
		error = -ENOMEM;
		goto fail2;
	}

	wacom->usbdev = dev;
	wacom->intf = intf;
	mutex_init(&wacom->lock);
	usb_make_path(dev, wacom->phys, sizeof(wacom->phys));
	strlcat(wacom->phys, "/input0", sizeof(wacom->phys));

	wacom_wac->input = input_dev;

	endpoint = &intf->cur_altsetting->endpoint[0].desc;

	/* Retrieve the physical and logical size for OEM devices */
	error = wacom_retrieve_hid_descriptor(intf, features);
	if (error)
		goto fail3;

	strlcpy(wacom_wac->name, features->name, sizeof(wacom_wac->name));

	if (features->type == TABLETPC || features->type == TABLETPC2FG ||
			 features->type == BAMBOO_PT) {
		/* Append the device type to the name */
		strlcat(wacom_wac->name,
			features->device_type == BTN_TOOL_PEN ?
				" Pen" : " Finger",
			sizeof(wacom_wac->name));
	}

	input_dev->name = wacom_wac->name;
	input_dev->private = wacom;
	input_dev->cdev.dev = &intf->dev;
	input_dev->open = wacom_open;
	input_dev->close = wacom_close;
	usb_to_input_id(dev, &input_dev->id);

	wacom_setup_input_capabilities(input_dev, wacom_wac);

	usb_fill_int_urb(wacom->irq, dev,
			 usb_rcvintpipe(dev, endpoint->bEndpointAddress),
			 wacom_wac->data, features->pktlen,
			 wacom_sys_irq, wacom, endpoint->bInterval);
	wacom->irq->transfer_dma = wacom->data_dma;
	wacom->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	error = wacom_initialize_leds(wacom);
	if (error)
		goto fail3;

	error = input_register_device(input_dev);
	if (error)
		goto fail4;

	/* Note that if query fails it is not a hard failure */
	wacom_query_tablet_data(intf, features);

	usb_set_intfdata(intf, wacom);
	return 0;

 fail4:	wacom_destroy_leds(wacom);
 fail3:	usb_free_urb(wacom->irq);
 fail2:	usb_buffer_free(dev, WACOM_PKGLEN_MAX, wacom_wac->data, wacom->data_dma);
 fail1:	input_free_device(input_dev);
	kfree(wacom);
	return error;
}

static void wacom_disconnect(struct usb_interface *intf)
{
	struct wacom *wacom = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);

	usb_kill_urb(wacom->irq);
	input_unregister_device(wacom->wacom_wac.input);
	wacom_destroy_leds(wacom);
	usb_free_urb(wacom->irq);
	usb_buffer_free(interface_to_usbdev(intf), WACOM_PKGLEN_MAX,
			wacom->wacom_wac.data, wacom->data_dma);
	kfree(wacom);
}

static int wacom_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct wacom *wacom = usb_get_intfdata(intf);

	mutex_lock(&wacom->lock);
	usb_kill_urb(wacom->irq);
	mutex_unlock(&wacom->lock);

	return 0;
}

static int wacom_resume(struct usb_interface *intf)
{
	struct wacom *wacom = usb_get_intfdata(intf);
	struct wacom_features *features = &wacom->wacom_wac.features;
	int rv;

	mutex_lock(&wacom->lock);

	/* switch to wacom mode first */
	wacom_query_tablet_data(intf, features);
	wacom_led_control(wacom);

	if (wacom->open)
		rv = usb_submit_urb(wacom->irq, GFP_NOIO);
	else
		rv = 0;
	mutex_unlock(&wacom->lock);

	return rv;
}

static struct usb_driver wacom_driver = {
	.name =		"wacom",
	.id_table =	wacom_ids,
	.probe =	wacom_probe,
	.disconnect =	wacom_disconnect,
	.suspend =	wacom_suspend,
	.resume =	wacom_resume,
};

static int __init wacom_init(void)
{
	int result;

	result = usb_register(&wacom_driver);
	if (result == 0)
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
		       DRIVER_DESC "\n");
	return result;
}

static void __exit wacom_exit(void)
{
	usb_deregister(&wacom_driver);
}

module_init(wacom_init);
module_exit(wacom_exit);
