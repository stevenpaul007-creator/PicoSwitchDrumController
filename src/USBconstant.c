#include "USBconstant.h"

#include <string.h>

#include "tusb.h"

// 设备描述符：告诉主机这是一个什么 USB 设备。
const uint8_t DevDesc[] = {
    0x12, 0x01,
    0x00, 0x02,              // USB 规范版本 2.0
    0x00, 0x00, 0x00,        // 设备类 / 子类 / 协议
    0x40,                    // EP0 最大包长
    0x0D, 0x0F, 0x92, 0x00,  // VID / PID
    0x00, 0x01,              // 设备版本号
    0x01, 0x02, 0x00,        // 字符串描述符索引：厂商 / 产品 / 序列号
    0x01                     // 配置数
};

const uint16_t DevDescLen = sizeof(DevDesc);

// 配置描述符：包含 HID 接口和 IN/OUT 端点。
const uint8_t CfgDesc[] = {
    0x09, 0x02, 0x29, 0x00,
    0x01, 0x01, 0x00, 0x80, 0xFA,
    0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x56, 0x00,
    0x07, 0x05, 0x02, 0x03, 0x40, 0x00, 0x01,
    0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01,
};

const uint16_t CfgDescLen = sizeof(CfgDesc);

// HID 报告描述符：定义按键、Hat、摇杆和厂商自定义区。
const uint8_t ReportDesc[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00,
    0x45, 0x01, 0x75, 0x01, 0x95, 0x10, 0x05, 0x09, 0x19, 0x01, 0x29, 0x10,
    0x81, 0x02, 0x05, 0x01, 0x25, 0x07, 0x46, 0x3b, 0x01, 0x75, 0x04, 0x95,
    0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81,
    0x01, 0x26, 0xff, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09,
    0x32, 0x09, 0x35, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0x06, 0x00, 0xff,
    0x09, 0x20, 0x95, 0x01, 0x81, 0x02, 0x0a, 0x21, 0x26, 0x95, 0x08, 0x91,
    0x02, 0xc0,
};

const uint16_t ReportDescLen = sizeof(ReportDesc);

// 字符串描述符：语言 ID / 厂商 / 产品 / 版本。
const uint8_t LangDes[] = {0x04, 0x03, 0x09, 0x04};
const uint16_t LangDesLen = sizeof(LangDes);

const uint8_t Manuf_Des[] = {
    0x1C, 0x03,
    'H', 0x00, 'O', 0x00, 'R', 0x00, 'I', 0x00, ' ', 0x00, 'C', 0x00, 'O', 0x00,
    '.', 0x00, 'L', 0x00, 'T', 0x00, 'D', 0x00, '.', 0x00
};
const uint16_t Manuf_DesLen = sizeof(Manuf_Des);

const uint8_t Prod_Des[] = {
    0x24, 0x03,
    'P', 0x00, 'O', 0x00, 'K', 0x00, 'K', 0x00, 'E', 0x00, 'N', 0x00, ' ', 0x00,
    'C', 0x00, 'O', 0x00, 'N', 0x00, 'T', 0x00, 'R', 0x00, 'O', 0x00, 'L', 0x00,
    'L', 0x00, 'E', 0x00, 'R', 0x00
};
const uint16_t Prod_DesLen = sizeof(Prod_Des);

static const uint8_t Version_Des[] = {
    0x08, 0x03,
    '1', 0x00, '.', 0x00, '0', 0x00
};
static const uint16_t Version_DesLen = sizeof(Version_Des);

// TinyUSB 字符串描述符转换缓存。
static uint16_t string_desc[32];

// 把 UTF-16LE 形式的静态字符串包装成 TinyUSB 需要的字符串描述符格式。
static const uint16_t *copy_utf16le_descriptor(const uint8_t *src) {
  const uint8_t total_len = src[0];
  const uint8_t char_count = (total_len >= 2) ? (uint8_t)((total_len - 2) / 2) : 0;

  string_desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 + char_count * 2));
  for (uint8_t i = 0; i < char_count; i++) {
    string_desc[1 + i] = (uint16_t)((uint16_t)src[2 + i * 2] | ((uint16_t)src[3 + i * 2] << 8));
  }
  return string_desc;
}

// 设备描述符回调。
const uint8_t *tud_descriptor_device_cb(void) {
  return DevDesc;
}

// 配置描述符回调。
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return CfgDesc;
}

// HID 报告描述符回调。
const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void)instance;
  return ReportDesc;
}

// 字符串描述符回调。
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;

  switch (index) {
    case 0:
      return copy_utf16le_descriptor(LangDes);
    case 1:
      return copy_utf16le_descriptor(Manuf_Des);
    case 2:
      return copy_utf16le_descriptor(Prod_Des);
    case 3:
      return copy_utf16le_descriptor(Version_Des);
    default:
      return NULL;
  }
}

// 主机向设备写 HID 报告时的回调；当前工程未使用下行报告。
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}

// 主机向设备读取 HID 报告时的回调；当前工程不提供额外输入报告。
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

// Earle Philhower Arduino core 的预编译 TinyUSB 配置启用了 MSC 类。
// 当前固件的配置描述符不暴露 MSC 接口；这些空实现只用于满足链接器，
// 实际枚举路径仍然只返回上面的自定义 HID 配置描述符。
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  (void)lun;
  (void)lba;
  (void)offset;
  (void)buffer;
  (void)bufsize;
  return -1;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  (void)lun;
  (void)lba;
  (void)offset;
  (void)buffer;
  (void)bufsize;
  return -1;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  (void)lun;
  memcpy(vendor_id, "PICO    ", 8);
  memcpy(product_id, "TAIKO HID       ", 16);
  memcpy(product_rev, "0001", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void)lun;
  return false;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
  (void)lun;
  *block_count = 0;
  *block_size = 512;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
  (void)lun;
  (void)scsi_cmd;
  (void)buffer;
  (void)bufsize;
  return -1;
}
