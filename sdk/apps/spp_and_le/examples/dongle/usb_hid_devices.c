#include "os/os_api.h"
#include "app_config.h"
#include "usb/device/usb_stack.h"
#include "usb/device/hid.h"
#include "usb_config.h"

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[USB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

// typedef for callback to main app
typedef u8 (*hid_rx_callback_t)(u8 *rx_data, u32 rx_len);

// variable holding the actual callback reference
static hid_rx_callback_t app_hid_callback = NULL;

// registration function for the USB HID callback
void usb_hid_set_rx_callback(void *cb)
{
    app_hid_callback = (hid_rx_callback_t)cb;
}

// USB HID descriptor
static const u8 sHIDDescriptor[] = {
    //InterfaceDescriptor:
    USB_DT_INTERFACE_SIZE,     // Length
    USB_DT_INTERFACE,          // DescriptorType
    0x00,                      // bInterface number (Set by code later)
    0x00,                      // AlternateSetting
    0x02,                      // NumEndpoint
    USB_CLASS_HID,             // Class
    0x00,                      // Subclass
    0x00,                      // Protocol
    0x00,                      // Interface Name

    //HIDDescriptor:
    0x09,                      // bLength
    USB_HID_DT_HID,            // bDescriptorType
    0x01, 0x02,                // bcdHID
    0x00,                      // bCountryCode
    0x01,                      // bNumDescriptors
    0x22,                      // bDescriptorType (Report)
    0,                         // LOW(ReportLength) (Set by code later)
    0,                         // HIGH(ReportLength) (Set by code later)

    //EndpointDescriptor (IN - Device to PC):
    USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType
    USB_DIR_IN | HID_EP_IN,     // bEndpointAddress (e.g. 0x81)
    USB_ENDPOINT_XFER_INT,      // Interrupt
    LOBYTE(MAXP_SIZE_HIDIN), HIBYTE(MAXP_SIZE_HIDIN),// Maximum packet size
    0x01,                       // Poll interval

    //EndpointDescriptor (OUT - PC to Device)
    USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType
    USB_DIR_OUT | HID_EP_OUT,   // bEndpointAddress (e.g. 0x01)
    USB_ENDPOINT_XFER_INT,      // Interrupt
    LOBYTE(MAXP_SIZE_HIDOUT), HIBYTE(MAXP_SIZE_HIDOUT),// Maximum packet size
    0x01,                       // Poll interval
};

// USB HID device report descriptor
static const u8 sHIDReportDesc[] = {
    // 1. Usage Page = 0xFF00 (Vendor Defined Page 1)
    0x06, 0x00, 0xFF,

    // 2. Usage = 0x01 (Vendor Usage 1)
    0x09, 0x01,

    // 3. Collection (Application)
    0xA1, 0x01,

        // --- INPUT REPORT (Jieli -> Computer) ---
    0x09, 0x01,        // Usage (Vendor Usage 1)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x00,  // Logical Maximum (255)
    0x75, 0x08,        // Report Size (8 bits)
    0x95, 0x40,        // Report Count (64 bytes)
    0x81, 0x02,        // Input (Data, Var, Abs)

    // --- OUTPUT REPORT (Computer -> Jieli) ---
    0x09, 0x01,        // Usage (Vendor Usage 1)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x00,  // Logical Maximum (255)
    0x75, 0x08,        // Report Size (8 bits)
    0x95, 0x40,        // Report Count (64 bytes)
    0x91, 0x02,        // Output (Data, Var, Abs)

    0xC0               // End Collection
};

static const u8 *hid_report_map;
static int hid_report_map_size = 0;
void usb_hid_set_repport_map(const u8 *map, int size)
{
    hid_report_map_size = size;
    hid_report_map = map;
}

void usb_init() {

    usb_hid_set_repport_map(sHIDReportDesc, sizeof(sHIDReportDesc));

}

void dongle_hid_output_handler(u8 report_type, u8 report_id, u8 *packet, u16 size)
{
    log_info("hid_data_output:type= %02x,report_id= %d,size=%d", report_type, report_id, size);
    put_buf(packet, size);
}

static u32 get_hid_report_desc_len(u32 index)
{
    ASSERT(hid_report_map_size != NULL, "%s\n", "report map err\n");
    return hid_report_map_size;
}
static void *get_hid_report_desc(u32 index)
{
    return hid_report_map;
}

static u8 *hid_ep_in_dma;
static u8 *hid_ep_out_dma;

static u32 hid_tx_data(struct usb_device_t *usb_device, const u8 *buffer, u32 len)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    return usb_g_intr_write(usb_id, HID_EP_IN, buffer, len);
}

// receives USB data, forwards it to the callback, and returns a response code
static void hid_rx_data(struct usb_device_t *usb_device, u32 ep)
{

    // convert device pointer to ID (standard Jieli boilerplate)
    const usb_dev usb_id = usb_device2id(usb_device);

    // buffer for incoming data
    u8 rx_buffer[64];

    // buffer for outgoing data
    static u8 response[64];

    // 3. read data from USB
    u32 rx_len = usb_g_intr_read(usb_id, ep, rx_buffer, 64, 0);

    if (rx_len > 0) {

        // call the main app
        response[0] = app_hid_callback(rx_buffer, rx_len);

        // send back the response from main app
        hid_tx_data(usb_device, response, 64);

    }
}

static void hid_endpoint_init(struct usb_device_t *usb_device, u32 itf)
{

    const usb_dev usb_id = usb_device2id(usb_device);

    // IN endpoint (sending to PC)
    usb_g_ep_config(usb_id, HID_EP_IN | USB_DIR_IN, USB_ENDPOINT_XFER_INT, 0, hid_ep_in_dma, MAXP_SIZE_HIDIN);
    usb_enable_ep(usb_id, HID_EP_IN);

    // OUT endpoint (receiving from PC)
    // hook up the interrupt handler
    usb_g_set_intr_hander(usb_id, 2, hid_rx_data);

    // use the DMA buffer we allocated
    usb_g_ep_config(usb_id, 2, USB_ENDPOINT_XFER_INT, 1, hid_ep_out_dma, MAXP_SIZE_HIDOUT);

    // enable USB endpoint
    usb_enable_ep(usb_id, 2);
}

u32 hid_register(const usb_dev usb_id)
{
    hid_ep_in_dma = usb_alloc_ep_dmabuffer(usb_id, HID_EP_IN | USB_DIR_IN, MAXP_SIZE_HIDIN);
    hid_ep_out_dma = usb_alloc_ep_dmabuffer(usb_id, HID_EP_OUT,MAXP_SIZE_HIDOUT);
    return 0;
}

void hid_release(const usb_dev usb_id)
{
    return ;
}

static void hid_reset(struct usb_device_t *usb_device, u32 itf)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    log_debug("%s", __func__);
#if USB_ROOT2
    usb_disable_ep(usb_id, HID_EP_IN);
#else
    hid_endpoint_init(usb_device, itf);
#endif
}

extern void dongle_hid_output_handler(u8 report_type, u8 report_id, u8 *packet, u16 size);
static u32 hid_recv_output_report(struct usb_device_t *usb_device, struct usb_ctrlrequest *setup)
{

    const usb_dev usb_id = usb_device2id(usb_device);
    u32 ret = 0;
    u8 read_ep[8];
    u8 mute;
    u16 volume = 0;
    usb_read_ep0(usb_id, read_ep, MIN(sizeof(read_ep), setup->wLength));
    ret = USB_EP0_STAGE_SETUP;
    put_buf(read_ep, 8);

    /* r_printf("######hid_recv:%d",setup->wLength); */
    /* put_buf(read_ep, setup->wLength); */
    dongle_hid_output_handler(0x52, read_ep[0], &read_ep[1], setup->wLength - 1);
    return ret;
}


static struct usb_device_t *hid_device_hdl = NULL;
static u32 hid_itf_hander(struct usb_device_t *usb_device, struct usb_ctrlrequest *req)
{

    if (req == -1) {
        return 0;
    }
    hid_device_hdl = usb_device;

    const usb_dev usb_id = usb_device2id(usb_device);
    u32 tx_len;
    u8 *tx_payload = usb_get_setup_buffer(usb_device);
    u32 bRequestType = req->bRequestType & USB_TYPE_MASK;

    printf("====== %s,%08x\n", __FUNCTION__, bRequestType);

    switch (bRequestType) {
    case USB_TYPE_STANDARD:
        switch (req->bRequest) {
        case USB_REQ_GET_DESCRIPTOR:
            switch (HIBYTE(req->wValue)) {
            case USB_HID_DT_HID:
                tx_payload = (u8 *)sHIDDescriptor + USB_DT_INTERFACE_SIZE;
                tx_len = 9;
                tx_payload = usb_set_data_payload(usb_device, req, tx_payload, tx_len);
                tx_payload[7] = LOBYTE(get_hid_report_desc_len(req->wIndex));
                tx_payload[8] = HIBYTE(get_hid_report_desc_len(req->wIndex));
                break;
            case USB_HID_DT_REPORT:
                hid_endpoint_init(usb_device, req->wIndex);
                tx_len = get_hid_report_desc_len(req->wIndex);
                tx_payload = get_hid_report_desc(req->wIndex);
                usb_set_data_payload(usb_device, req, tx_payload, tx_len);
                break;
            }// USB_REQ_GET_DESCRIPTOR
            break;
        case USB_REQ_SET_DESCRIPTOR:
            usb_set_setup_phase(usb_device, USB_EP0_STAGE_SETUP);
            break;
        case USB_REQ_SET_INTERFACE:
            if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_CONFIGURED) {
                //只有一个interface 没有Alternate
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            }
            break;
        case USB_REQ_GET_INTERFACE:
            if (req->wLength) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_CONFIGURED) {
                tx_len = 1;
                tx_payload[0] = 0x00;
                usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            }
            break;
        case USB_REQ_GET_STATUS:
            if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            }
            break;
        }//bRequest @ USB_TYPE_STANDARD
        break;

    case USB_TYPE_CLASS: {
        switch (req->bRequest) {
        case USB_REQ_SET_IDLE:
            usb_set_setup_phase(usb_device, USB_EP0_STAGE_SETUP);
            break;
        case USB_REQ_GET_IDLE:
            tx_len = 1;
            tx_payload[0] = 0;
            usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            break;
        case USB_REQ_SET_REPORT:
            usb_set_setup_recv(usb_device, hid_recv_output_report);
            break;
        }//bRequest @ USB_TYPE_CLASS
    }
    break;
    }
    return 0;
}

u32 hid_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num)
{
    log_debug("hid interface num:%d\n", *cur_itf_num);
    u8 *_ptr = ptr;
    memcpy(ptr, sHIDDescriptor, sizeof(sHIDDescriptor));
    ptr[2] = *cur_itf_num;
    if (usb_set_interface_hander(usb_id, *cur_itf_num, hid_itf_hander) != *cur_itf_num) {
        ASSERT(0, "hid set interface_hander fail");
    }

    if (usb_set_reset_hander(usb_id, *cur_itf_num, hid_reset) != *cur_itf_num) {
        ASSERT(0, "hid set interface_reset_hander fail");
    }

    ptr[USB_DT_INTERFACE_SIZE + 7] = LOBYTE(get_hid_report_desc_len(0));
    ptr[USB_DT_INTERFACE_SIZE + 8] = HIBYTE(get_hid_report_desc_len(0));
    *cur_itf_num = *cur_itf_num + 1;
    return sizeof(sHIDDescriptor) ;
}

u32 hid_send_data(const void *p, u32 len)
{
    if (!hid_device_hdl) {
        return 0;
    }
    return hid_tx_data(hid_device_hdl, p, len);
}

