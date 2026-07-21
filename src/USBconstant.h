#ifndef __USB_CONST_DATA_H__
#define __USB_CONST_DATA_H__

#include <stdint.h>

// USB 描述符原始数据：设备描述符、配置描述符、字符串描述符、HID 报告描述符。
extern const uint8_t DevDesc[];
extern const uint8_t CfgDesc[];
extern const uint8_t LangDes[];
extern const uint8_t ReportDesc[];
extern const uint8_t Prod_Des[];
extern const uint8_t Manuf_Des[];

// 各描述符长度，供 TinyUSB 回调使用。
extern const uint16_t DevDescLen;
extern const uint16_t CfgDescLen;
extern const uint16_t LangDesLen;
extern const uint16_t ReportDescLen;
extern const uint16_t Prod_DesLen;
extern const uint16_t Manuf_DesLen;

#endif
