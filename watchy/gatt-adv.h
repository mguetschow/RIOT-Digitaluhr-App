#ifndef _WATCHY_GATT_H
#define _WATCHY_GATT_H

int watchy_gatt_init (void);

// *buf pointer to an allocated buffer of length len
// returns number of bytes written to buf
// buf does not need to be larger than 21 bytes,
// which is the NUS maxuimum per NUS transfer (plus trailing NUL byte)
int watchy_gatt_nus_get_rx(char *buf, unsigned int len);

ssize_t gatt_svr_nus_tx_buf(char *buf, unsigned int len);

// returns alert Level of last ialert, only one byte, no text or data
//     0x00 No Alert
//     0x01 Mild Alert
//     0x02 High Alert
uint8_t watchy_gatt_get_ialert(void);

// return pointer to structure containting the last alert
alert_t *watchy_gatt_get_alert(void);

#endif
