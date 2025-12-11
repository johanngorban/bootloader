#include "eeprom.h"
#include "usbd_conf.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"
#include "usbd_customhid.h"
#include "log.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

uint8_t i2c_status = STATUS_ADDRESS_ACK;

uint8_t HID_Out_Buffer[64];

uint8_t eeprom(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint8_t recipient = pdev->request.bmRequest;
    uint8_t cmd = pdev->request.bRequest;
    uint16_t addr = pdev->request.wIndex;
    uint8_t is_read = pdev->request.wValue & I2C_M_RD; // uint16_t
    uint16_t size = pdev->request.wLength;
	  mes(OK, "USB", "Req %x cmd_io %x rw %x addr %x len %x"CRLF, pdev->request.bmRequest, pdev->request.bRequest, pdev->request.wValue, addr, pdev->request.wLength);
		
		
		if (recipient != 0x41 && recipient != 0x40 && recipient != 0xc0 && recipient != 0xc1 && recipient != 0xa0 && recipient != 0x20 && recipient != 0x21) {
			return 0;
		}
		
		
		
		USBD_CUSTOM_HID_HandleTypeDef     *hhid;
		
		hhid = (USBD_CUSTOM_HID_HandleTypeDef *) pdev->pClassData;
		
		mes(INFO, "USB", "hid 0x%x %x"CRLF, hhid->Report_buf[0], hhid->Report_buf[1]);
		
		if (cmd == CMD_GET_FUNC)
    {
        uint32_t func = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
				mes(INFO, "USB", "Send func 0x%x %d"CRLF, func, sizeof(func));
				USBD_CtlSendData(pdev, (uint8_t *)&func, sizeof(func));
        return 1;
    }
		
		if (cmd == CMD_SET_DELAY)
    {
				mes(INFO, "USB", "Send CMD_SET_DELAY %d"CRLF, i2c_status);
        return 1;
    }
		
		if (cmd == CMD_GET_STATUS)
    {
				mes(INFO, "USB", "Send status %d"CRLF, i2c_status);
        USBD_CtlSendData(pdev, &i2c_status, sizeof(i2c_status));
        return  1;
    }
		
		mes(INFO, "USB", "Send data"CRLF);
		uint8_t data[15] = {cmd,0x00,0xFF,0xFF,0xFF};
		USBD_CtlSendData(pdev, data, 2);
		i2c_status = STATUS_ADDRESS_ACK;
		return 1;
		
	
		
	
/*
    
        usb_read(adapter, CMD_GET_FUNC, 0,   0,   pfunc, sizeof(*pfunc)) != sizeof(*pfunc)
        usb_read(adapter,    cmd,       rw, addr, data,  len)
        usb_control_msg(ad, ad, cmd, USB_TYPE_VENDOR | USB_RECIP_INTERFACE | USB_DIR_IN, rw, addr, data, len, 2000)
        0x40              0x01                  0x80
        USB_TYPE_VENDOR | USB_RECIP_INTERFACE | USB_DIR_IN = 0xC1
    
    if (cmd == CMD_GET_FUNC)
    {
        uint32_t func = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
        // pInformation->Ctrl_Info.Usb_wLength = sizeof(func); отправляемый размер
				mes(INFO, "USB", "Send func 0x%x %d"CRLF, func, sizeof(func));
				USBD_CtlSendData(&hUsbDeviceFS, (uint8_t *)&func, sizeof(func));
        return 1;
    }
    
    usb_read(adapter, CMD_GET_STATUS, 0, 0, pstatus, 1) != 1
		CMD_GET_STATUS = 3
    
    if (cmd == CMD_GET_STATUS)
    {
				mes(INFO, "USB", "Send status %d"CRLF, i2c_status);
        USBD_LL_Transmit(pdev, 0x01U, &i2c_status, sizeof(i2c_status));
        return  0;
    }
		if (cmd == CMD_SET_DELAY)
    {
				mes(INFO, "USB", "Send CMD_SET_DELAY %d"CRLF, i2c_status);
        return 1;
    }
    if (cmd == CMD_I2C_IO || cmd == (CMD_I2C_IO | CMD_I2C_BEGIN) || cmd == (CMD_I2C_IO | CMD_I2C_END) || cmd == (CMD_I2C_IO | CMD_I2C_BEGIN | CMD_I2C_END))
    {
				
        i2c_status = STATUS_ADDRESS_NACK;
        if (addr == 0x68)
        {
            //eeprom_16bit_emul(ptr, Length);
					
						mes(INFO, "USB", "Send data 0x68 size %d"CRLF, size);
						uint8_t data[15] = {1,0x01,1,1,1,1,1,1,1};
					
						USBD_LL_Transmit(pdev, 0x81U, data, size);
						i2c_status = STATUS_ADDRESS_ACK;
						return 1;

            
        } else
        if (addr == 0x48)
        {
            //eeprom_16bit_emul(ptr, Length);

					0xAC
					
						uint8_t data[15] = {0x02,0x01,1,1,1};
						//if (size > 0) {
							//dump(psetup, 32, 1);
							//dump(pdev->pUserData, 32, 1);
							//dump(pdev->pClassData, 32, 1);
					//	}
						//mes(INFO, "USB", "Send data size %d"CRLF, size);
						//USBD_LL_PrepareReceive(pdev, HID_EPOUT_ADDR, (uint8_t*)HID_Out_Buffer, HID_EPOUT_SIZE);
						//USBD_LL_Transmit(pdev, 0x81U, data, 1);
					
						USBD_CtlSendData(&hUsbDeviceFS, data, 5);
						i2c_status = STATUS_ADDRESS_ACK;
						return 1;

            
        }else
        {
            i2c_status = STATUS_ADDRESS_NACK;
        }
    }
		*/
    return 0;
}