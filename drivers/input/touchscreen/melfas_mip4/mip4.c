/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4.c : Main functions
 *
 *
 * Version : 2015.08.23
 *
 */
#include "mip4.h"
#if MIP_USE_WAKEUP_GESTURE
struct wake_lock mip_wake_lock;
u8 mip_wakeup_gesture_enable = 0;
#endif

/* add chip info  wangli_20151215 */
#define SYS_ATTR_FILE_MIP4
#ifdef SYS_ATTR_FILE_MIP4
static struct kobject *android_touch_kobj;
u8 *sys_fw_ver = NULL;
#endif
/* add chip info  wangli_20151215 */

/**
* Reboot chip
*
* Caution : IRQ must be disabled before mip_reboot and enabled after mip_reboot.
*/
void mip_reboot(struct mip_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
		
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	i2c_lock_adapter(adapter);
	
	mip_power_off(info);
	mip_power_on(info);

	i2c_unlock_adapter(adapter);

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/**
* I2C Read
*/
int mip_i2c_read(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = read_len,
		},
	};

	while(retry--){	
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if(res == ARRAY_SIZE(msg)){
			goto DONE;
		}
		else if(res < 0){
			//dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		}
		else if(res != ARRAY_SIZE(msg)){
			//dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}			
		else{
			//dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	goto ERROR_REBOOT;
	
ERROR_REBOOT:
#if RESET_ON_I2C_ERROR
	mip_reboot(info);
#endif
	return 1;
	
DONE:
	return 0;
}


/**
* I2C Read (Continue)
*/
int mip_i2c_read_next(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, int start_idx, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;
	u8 rbuf[read_len];

	/*
	while(retry--){
		res = i2c_master_recv(info->client, rbuf, read_len);
		
		if(res == read_len){
			goto DONE;
		}
		else if(res < 0){
			//dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv - errno [%d]\n", __func__, res);
		}
		else if(res != read_len){
			//dev_err(&info->client->dev, "%s [ERROR] length mismatch - read[%d] result[%d]\n", __func__, read_len, res);
		}			
		else{
			//dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	*/

	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = read_len,
		},
	};
	
	while(retry--){	
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if(res == ARRAY_SIZE(msg)){
			goto DONE;
		}
		else if(res < 0){
			//dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		}
		else if(res != ARRAY_SIZE(msg)){
			//dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}			
		else{
			//dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	
	goto ERROR_REBOOT;
	
ERROR_REBOOT:
#if RESET_ON_I2C_ERROR
	mip_reboot(info);
#endif
	return 1;

DONE:
	memcpy(&read_buf[start_idx], rbuf, read_len);
	
	return 0;
}

/**
* I2C Write
*/
int mip_i2c_write(struct mip_ts_info *info, char *write_buf, unsigned int write_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	while(retry--){
		res = i2c_master_send(info->client, write_buf, write_len);

		if(res == write_len){
			goto DONE;
		}
		else if(res < 0){
			//dev_err(&info->client->dev, "%s [ERROR] i2c_master_send - errno [%d]\n", __func__, res);
		}
		else if(res != write_len){
			//dev_err(&info->client->dev, "%s [ERROR] length mismatch - write[%d] result[%d]\n", __func__, write_len, res);
		}			
		else{
			//dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	
	goto ERROR_REBOOT;
	
ERROR_REBOOT:
#if RESET_ON_I2C_ERROR
	mip_reboot(info);
#endif
	return 1;
	
DONE:
	return 0;
}

/**
* Enable device
*/
int mip_enable(struct mip_ts_info *info)
{
	//dev_dbg(&info->client->dev, "%s [START] %d \n", __func__, info->enabled);

	if (info->enabled){
		//dev_err(&info->client->dev, "%s [ERROR] device already enabled\n", __func__);
		goto EXIT;
	}

	mutex_lock(&info->lock);
#if MIP_USE_WAKEUP_GESTURE
	if (mip_wakeup_gesture_enable){
	#if 0    //@hardy 20151127 
	mip_set_power_state(info, MIP_CTRL_POWER_ACTIVE);
	#else
		mip_power_off(info);
		msleep(10);
		mip_power_on(info);
	#endif
	if(wake_lock_active(&mip_wake_lock)){
//		wake_unlock(&mip_wake_lock);
//		dev_info(&info->client->dev, "%s - wake_unlock\n", __func__);
	}
	
	info->nap_mode = false;
	//dev_dbg(&info->client->dev, "%s - nap mode : off\n", __func__);

		if(!info->irq_flag){
			enable_irq(info->client->irq);
			info->irq_flag = 1;
		}

		if (device_may_wakeup(&info->client->dev)){
			if (info->irq_wake_flag){
				disable_irq_wake(info->client->irq);
				info->irq_wake_flag = 0;
			}
		}
	}
	else
#endif
	{
		mip_power_on(info);
		enable_irq(info->client->irq);
		info->irq_flag = 1;
	}

#if 1
	if(info->disable_esd == true){
		//Disable ESD alert
		mip_disable_esd_alert(info);
	}	
#endif

	info->enabled = true;
	mutex_unlock(&info->lock);

EXIT:
	//dev_info(&info->client->dev, MIP_DEVICE_NAME" - Enabled\n");
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Disable device
*/
int mip_disable(struct mip_ts_info *info)
{
	//dev_dbg(&info->client->dev, "%s [START] %d\n", __func__, info->enabled);

	if (!info->enabled){
		//dev_err(&info->client->dev, "%s [ERROR] device already disabled\n", __func__);
		goto EXIT;
	}

	mip_clear_input(info);

	mutex_lock(&info->lock);
#if MIP_USE_WAKEUP_GESTURE
	if (mip_wakeup_gesture_enable){
		info->wakeup_gesture_code = 0;

		//mip_set_wakeup_gesture_type(info, MIP_EVENT_GESTURE_ALL);
		mip_set_power_state(info, MIP_CTRL_POWER_LOW);

		info->nap_mode = true;
		//dev_dbg(&info->client->dev, "%s - nap mode : on\n", __func__);

		if(!wake_lock_active(&mip_wake_lock)) {
//			wake_lock(&mip_wake_lock);
//			dev_info(&info->client->dev, "%s - wake_lock\n", __func__);
		}

		if (device_may_wakeup(&info->client->dev)){
			enable_irq_wake(info->client->irq);
			info->irq_wake_flag = 1;
		}
	}
	else
#endif
	{
		disable_irq(info->client->irq);
		mip_power_off(info);
		info->irq_flag = 0;
	}

	info->enabled = false;
	mutex_unlock(&info->lock);

EXIT:	
	//dev_info(&info->client->dev, MIP_DEVICE_NAME" - Disabled\n");
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

#if MIP_USE_INPUT_OPEN_CLOSE
/**
* Open input device
*/
static int mip_input_open(struct input_dev *dev) 
{
	struct mip_ts_info *info = input_get_drvdata(dev);
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if(info->init == true){
		info->init = false;
	} 
	else{
		mip_enable(info);
	}

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	
	return 0;
}

/**
* Close input device
*/
static void mip_input_close(struct input_dev *dev) 
{
	struct mip_ts_info *info = input_get_drvdata(dev);

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip_disable(info);

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return;
}
#endif

/**
* Get ready status
*/
int mip_get_ready_status(struct mip_ts_info *info)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;
	
	////dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_READY_STATUS;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		//dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}
	ret = rbuf[0];

	//check status
	if((ret == MIP_CTRL_STATUS_NONE) || (ret == MIP_CTRL_STATUS_LOG) || (ret == MIP_CTRL_STATUS_READY)){
		//dev_dbg(&info->client->dev, "%s - status [0x%02X]\n", __func__, ret);
	}
	else{
		//dev_err(&info->client->dev, "%s [ERROR] Unknown status [0x%02X]\n", __func__, ret);
		goto ERROR;
	}

	if(ret == MIP_CTRL_STATUS_LOG){
		//skip log event
		wbuf[0] = MIP_R0_LOG;
		wbuf[1] = MIP_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		if(mip_i2c_write(info, wbuf, 3)){
			//dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		}
	}
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
	
ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Read chip firmware version
*/
int mip_get_fw_version(struct mip_ts_info *info, u8 *ver_buf, int buf_size)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int i;
	
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_BOOT;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 8)){
		goto ERROR;
	};

	for(i = 0; i < MIP_FW_MAX_SECT_NUM; i++){
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}	
	
	return 0;

ERROR:
	memset(ver_buf, 0xFF, buf_size);
	
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Read chip firmware version for u16
*/
int mip_get_fw_version_u16(struct mip_ts_info *info, u16 *ver_buf_u16, int buf_size)
{
	u8 rbuf[8];
	int i;
	
	if(mip_get_fw_version(info, rbuf, buf_size)){
		goto ERROR;
	}

	for(i = 0; i < MIP_FW_MAX_SECT_NUM; i++){
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];
	}	
	
	return 0;

ERROR:
	memset(ver_buf_u16, 0xFFFF, buf_size);
	
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Read bin(file) firmware version
*/
int mip_get_fw_version_from_bin(struct mip_ts_info *info, u8 *ver_buf, int buf_size)
{
	const struct firmware *fw;

	//dev_dbg(&info->client->dev,"%s [START]\n", __func__);

	request_firmware(&fw, FW_PATH_INTERNAL, &info->client->dev);

	if(!fw) {
		//dev_err(&info->client->dev,"%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	if(mip_bin_fw_version(info, fw->data, fw->size, ver_buf)){
		memset(ver_buf, 0xFF, buf_size);
		//dev_err(&info->client->dev,"%s [ERROR] mip_bin_fw_version\n", __func__);
		goto ERROR;
	}
	
	release_firmware(fw);
	
	//dev_dbg(&info->client->dev,"%s [DONE]\n", __func__);
	return 0;	
	
ERROR:
	//dev_err(&info->client->dev,"%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Set power state
*/
int mip_set_power_state(struct mip_ts_info *info, u8 mode)
{
	u8 wbuf[3];

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//dev_dbg(&info->client->dev, "%s - mode[%02X]\n", __func__, mode);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_POWER_STATE;
	wbuf[2] = mode;
	if(mip_i2c_write(info, wbuf, 3)){
		//dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Set wake-up gesture type
*/
int mip_set_wakeup_gesture_type(struct mip_ts_info *info, u32 type)
{
	u8 wbuf[6];

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//dev_dbg(&info->client->dev, "%s - type[%08X]\n", __func__, type);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_GESTURE_TYPE;
	wbuf[2] = (type >> 24) & 0xFF;
	wbuf[3] = (type >> 16) & 0xFF;
	wbuf[4] = (type >> 8) & 0xFF;
	wbuf[5] = type & 0xFF;
	if(mip_i2c_write(info, wbuf, 6)){
		//dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Disable ESD alert
*/
int mip_disable_esd_alert(struct mip_ts_info *info)
{
	u8 wbuf[4];
	u8 rbuf[4];
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_DISABLE_ESD_ALERT;
	wbuf[2] = 1;
	if(mip_i2c_write(info, wbuf, 3)){
		//dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		//dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}	

	if(rbuf[0] != 1){
		//dev_dbg(&info->client->dev, "%s [ERROR] failed\n", __func__);
		goto ERROR;
	}
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - ESD
*/
static int mip_alert_handler_esd(struct mip_ts_info *info, u8 *rbuf)
{
	u8 frame_cnt = rbuf[1];
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//dev_dbg(&info->client->dev, "%s - frame_cnt[%d]\n", __func__, frame_cnt);

	if(frame_cnt == 0){
		//sensor crack, not ESD
		info->esd_cnt++;
		//dev_dbg(&info->client->dev, "%s - esd_cnt[%d]\n", __func__, info->esd_cnt);

		if(info->disable_esd == true){
			mip_disable_esd_alert(info);
			info->esd_cnt = 0;
		}
		else if(info->esd_cnt > ESD_COUNT_FOR_DISABLE){
			//Disable ESD alert
			if(mip_disable_esd_alert(info)){
			}
			else{
				info->disable_esd = true;
				info->esd_cnt = 0;
			}
		}
		else{
			//Reset chip
			mip_reboot(info);
		}
	}
	else{
		//ESD detected
		//Reset chip
		mip_reboot(info);
		info->esd_cnt = 0;
	}

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:	
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	//return 1;
}

/**
* Alert event handler - Wake-up
*/
static int mip_alert_handler_wakeup(struct mip_ts_info *info, u8 *rbuf)
{
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if(mip_wakeup_event_handler(info, rbuf)){
		goto ERROR;
	}

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - Input type
*/
static int mip_alert_handler_inputtype(struct mip_ts_info *info, u8 *rbuf)
{
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//...
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
//ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - 0xF1
*/
static int mip_alert_handler_F1(struct mip_ts_info *info, u8 *rbuf, u8 size)
{
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

#if MIP_USE_LPWG
	if(mip_lpwg_event_handler(info, rbuf, size)){
		//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
		return 1;
	}
#endif
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Interrupt handler
*/
static irqreturn_t mip_interrupt(int irq, void *dev_id)
{
	struct mip_ts_info *info = dev_id;
	//struct i2c_client *client = info->client;
	u8 wbuf[8];
	u8 rbuf[256];
	unsigned int size = 0;
	//int event_size = info->event_size;
	u8 category = 0;
	u8 alert_type = 0;
	
	//dev_dbg(&client->dev, "%s [START]\n", __func__);
	
	//Read packet info
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_INFO;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		//dev_err(&client->dev, "%s [ERROR] Read packet info\n", __func__);
		goto ERROR;
	}

	size = (rbuf[0] & 0x7F);	
	category = ((rbuf[0] >> 7) & 0x1);
	//dev_dbg(&client->dev, "%s - packet info : size[%d] category[%d]\n", __func__, size, category);	
	
	//Check size
	if(size <= 0){
		//dev_err(&client->dev, "%s [ERROR] Packet size [%d]\n", __func__, size);
		goto EXIT;
	}

	//Read packet data
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_DATA;
	if(mip_i2c_read(info, wbuf, 2, rbuf, size)){
		//dev_err(&client->dev, "%s [ERROR] Read packet data\n", __func__);
		goto ERROR;
	}

	//Event handler
	if(category == 0){
		//Touch event
		info->esd_cnt = 0;
		
		mip_input_event_handler(info, size, rbuf);
	}
	else{
		//Alert event
		alert_type = rbuf[0];
		
		//dev_dbg(&client->dev, "%s - alert type [%d]\n", __func__, alert_type);
				
		if(alert_type == MIP_ALERT_ESD){
			//ESD detection
			if(mip_alert_handler_esd(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_ALERT_WAKEUP){
			//Wake-up gesture
			if(mip_alert_handler_wakeup(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_ALERT_INPUT_TYPE){
			//Input type changed
			if(mip_alert_handler_inputtype(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_ALERT_F1){
			//0xF1
			if(mip_alert_handler_F1(info, rbuf, size)){
				goto ERROR;
			}
		}
		else{
			//dev_err(&client->dev, "%s [ERROR] Unknown alert type [%d]\n", __func__, alert_type);
			goto ERROR;
		}		
	}

EXIT:
	//dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return IRQ_HANDLED;
	
ERROR:
	if(RESET_ON_EVENT_ERROR){	
		//dev_info(&client->dev, "%s - Reset on error\n", __func__);
		
		mip_disable(info);
		mip_clear_input(info);
		mip_enable(info);
	}

	//dev_err(&client->dev, "%s [ERROR]\n", __func__);
	return IRQ_HANDLED;
}

/**
* Update firmware from kernel built-in binary
*/
int mip_fw_update_from_kernel(struct mip_ts_info *info)
{
	const char *fw_name = FW_PATH_INTERNAL;
	const struct firmware *fw;
	int retires = 3;
	int ret = fw_err_none;
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ	
	//mutex_lock(&info->lock);	
	//disable_irq(info->client->irq);

	//Get firmware
	request_firmware(&fw, fw_name, &info->client->dev);
	
	if (!fw) {
		//dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	//Update firmware
	do {
		ret = mip_flash_fw(info, fw->data, fw->size, false, true);
		if(ret >= fw_err_none){
			break;
		}
	} while (--retires);

	if (!retires) {
		//dev_err(&info->client->dev, "%s [ERROR] mip_flash_fw failed\n", __func__);
		ret = fw_err_download;
	}
	
	release_firmware(fw);

	//Enable IRQ
	//enable_irq(info->client->irq);	
	//mutex_unlock(&info->lock);

	if(ret < fw_err_none){
		goto ERROR;
	}
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Update firmware from external storage
*/
int mip_fw_update_from_storage(struct mip_ts_info *info, char *path, bool force)
{
	struct file *fp; 
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int ret = fw_err_none;
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);	
 	disable_irq(info->client->irq);

	//Get firmware
	old_fs = get_fs();
	set_fs(KERNEL_DS);  

	fp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		//dev_err(&info->client->dev, "%s [ERROR] file_open - path[%s]\n", __func__, path);
		ret = fw_err_file_open;
		goto ERROR;
	}
	
 	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		//Read firmware
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		//dev_dbg(&info->client->dev, "%s - path[%s] size[%u]\n", __func__, path, fw_size);
		
		if (nread != fw_size) {
			//dev_err(&info->client->dev, "%s [ERROR] vfs_read - size[%d] read[%d]\n", __func__, fw_size, nread);
			ret = fw_err_file_read;
		}
		else{
			//Update firmware
			ret = mip_flash_fw(info, fw_data, fw_size, force, true);
		}
		
		kfree(fw_data);
	}
	else{
		//dev_err(&info->client->dev, "%s [ERROR] fw_size [%d]\n", __func__, fw_size);
		ret = fw_err_file_read;
	}
	
 	filp_close(fp, current->files);

ERROR:
	set_fs(old_fs);	

	//Enable IRQ
	enable_irq(info->client->irq);	
	mutex_unlock(&info->lock);

	if(ret < fw_err_none){
		//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	}
	else{
		//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	}
	
	return ret;
}

static ssize_t mip_sys_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int result = 0;
	u8 data[255];
	int ret = 0;
	
	memset(info->print_buf, 0, PAGE_SIZE);

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	ret = mip_fw_update_from_storage(info, info->fw_path_ext, true);
	
	switch(ret){
		case fw_err_none:
			sprintf(data, "F/W update success.\n");
			break;
		case fw_err_uptodate:
			sprintf(data, "F/W is already up-to-date.\n");
			break;
		case fw_err_download:
			sprintf(data, "F/W update failed : Download error\n");
			break;
		case fw_err_file_type:
			sprintf(data, "F/W update failed : File type error\n");
			break;
		case fw_err_file_open:			
			sprintf(data, "F/W update failed : File open error [%s]\n", info->fw_path_ext);
			break;
		case fw_err_file_read:
			sprintf(data, "F/W update failed : File read error\n");
			break;
		default:
			sprintf(data, "F/W update failed.\n");
			break;
	}
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	
	strcat(info->print_buf, data);
	result = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return result;
}
static DEVICE_ATTR(fw_update, 0444, mip_sys_fw_update, NULL);

/**
* Sysfs attr info
*/
static struct attribute *mip_attrs[] = {
	&dev_attr_fw_update.attr,
	NULL,
};

/**
* Sysfs attr group info
*/
static const struct attribute_group mip_attr_group = {
	.attrs = mip_attrs,
};

/****************** add chip info start wangli_20151215 *******************/
#ifdef SYS_ATTR_FILE_MIP4
static ssize_t show_chipinfo(struct device *dev,struct device_attribute *attr,char *buf)
{
	ssize_t ret;

	ret = sprintf(buf,"IC:melfas_MMS438,fw_ver:%02X.%02X/%02X.%02X/%02X.%02X/%02X.%02X\n",
					sys_fw_ver[0],sys_fw_ver[1],sys_fw_ver[2],
					sys_fw_ver[3],sys_fw_ver[4],sys_fw_ver[5],
					sys_fw_ver[6],sys_fw_ver[7]);
//    printk("[melfas] %s IC:melfas_MMS438,fw_ver:%02X.%02X/%02X.%02X/%02X.%02X/%02X.%02X\n",
//					__func__, sys_fw_ver[0],sys_fw_ver[1],
//					sys_fw_ver[2],sys_fw_ver[3],sys_fw_ver[4],
//					sys_fw_ver[5],sys_fw_ver[6],sys_fw_ver[7]);
	return ret;
}
static DEVICE_ATTR(info,0444,show_chipinfo,NULL);

#if MIP_USE_WAKEUP_GESTURE
static ssize_t show_wakeup_gesture_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf,"melfas_MMS438 wakeup_gesture_enable = %d\n", mip_wakeup_gesture_enable);

	return ret;
}

static ssize_t store_wakeup_gesture_status(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	int mode, ret;

	ret = kstrtoint(buf, 10, &mode);
	if (ret)
		return ret;

	mip_wakeup_gesture_enable = mode;
	printk(KERN_ERR "[melfas_MMS438]%s: mip_wakeup_gesture_enable = %d\n",__func__, mip_wakeup_gesture_enable);

	return count;
}

static DEVICE_ATTR(wakeup_gesture_enable,0664,show_wakeup_gesture_status,store_wakeup_gesture_status);
#endif

/**
* Sysfs attr info2
*/
static struct attribute *mip_info_attrs[] = {
	&dev_attr_info.attr,
#if MIP_USE_WAKEUP_GESTURE
	&dev_attr_wakeup_gesture_enable.attr,
#endif
	NULL,
};

/**
* Sysfs attr group info2
*/
static const struct attribute_group mip_info_attr_group[] = {
    {.attrs = mip_info_attrs},
};

static void add_sysfs_android_touch(void)
{
    int ret;

    android_touch_kobj = kobject_create_and_add("android_touch", NULL);
    if (android_touch_kobj == NULL)
    {
        printk(KERN_ERR "[melfas_MMS438]%s: kobject_create_and_add failed\n", __func__);
        return;
    }

    ret = sysfs_create_group(android_touch_kobj, mip_info_attr_group);
    if (ret < 0)
    {
        printk(KERN_ERR "[melfas_MMS438]%s: sysfs_create_group failed\n",__func__);
    }
    return;
}
static void remove_sysfs_android_touch(void)
{
    if(android_touch_kobj)
    {
        sysfs_remove_group(android_touch_kobj,mip_info_attr_group);
        kobject_put(android_touch_kobj);
    }
}
#endif
/****************** add chip info end wangli_20151215 ********************/

/*******************************************************
Function:
	I2c test Function.
Input:
	client:i2c client.
Output:
	Executive outcomes.
	0: succeed, otherwise failed.
*******************************************************/
static int mip_i2c_test(struct mip_ts_info *info)
{
    int ret = -EIO;
    u8 wbuf[8];
    u8 rbuf[64];
    int retry = 1;
    //Product name
    wbuf[0] = MIP_R0_INFO;
    wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
    while (retry--) {
        ret = mip_i2c_read(info, wbuf, 2, rbuf, 16);
        if (ret == 0){
            memcpy(info->product_name, rbuf, 16);
            dev_err(&info->client->dev, "%s - product_name[%s]\n", __func__, info->product_name);
            return ret;
        }
        dev_err(&info->client->dev, "mip_i2c_test time %d.\n", retry);
        msleep(20);
    }

    return -EIO;
}
/**
* Initial config
*/
static int mip_init_config(struct mip_ts_info *info)
{
	u8 wbuf[8];
	u8 rbuf[64];
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Product name
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	mip_i2c_read(info, wbuf, 2, rbuf, 16);
	memcpy(info->product_name, rbuf, 16);
	dev_info(&info->client->dev, "%s - product_name[%s]\n", __func__, info->product_name);

	//Firmware version
	mip_get_fw_version(info, rbuf, 32);
	memcpy(info->fw_version, rbuf, 8);
	//dev_info(&info->client->dev, "%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", __func__, info->fw_version[0], info->fw_version[1], info->fw_version[2], info->fw_version[3], info->fw_version[4], info->fw_version[5], info->fw_version[6], info->fw_version[7]);	

	//Resolution
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);

#if MIP_AUTOSET_RESOLUTION
	//Set resolution using chip info
	info->max_x = (rbuf[0]) | (rbuf[1] << 8);
	info->max_y = (rbuf[2]) | (rbuf[3] << 8);
#else
	//Set resolution using platform data
	info->max_x = info->pdata->max_x;
	info->max_y = info->pdata->max_y;
#endif

	dev_info(&info->client->dev, "%s - max_x[%d] max_y[%d]\n", __func__, info->max_x, info->max_y);

	//Node info
	info->node_x = rbuf[4];
	info->node_y = rbuf[5];
	info->node_key = rbuf[6];
	dev_info(&info->client->dev, "%s - node_x[%d] node_y[%d] node_key[%d]\n", __func__, info->node_x, info->node_y, info->node_key);

	//Key info
	if(info->node_key > 0){
		//Enable touchkey
		info->key_enable = true;
		info->key_num = info->node_key;
	}

	//Protocol
#if MIP_AUTOSET_EVENT_FORMAT
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_SUPPORTED_FUNC;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);
	info->event_format = (rbuf[4]) | (rbuf[5] << 8);
	info->event_size = rbuf[6];
#else
	info->event_format = 0;
	info->event_size = 6;
#endif
	//dev_dbg(&info->client->dev, "%s - event_format[%d] event_size[%d] \n", __func__, info->event_format, info->event_size);
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
//ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	//return 1;
}

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct mip_ts_info *melfas_data =
		container_of(self, struct mip_ts_info, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
			melfas_data && melfas_data->client) {
		blank = evdata->data;
#if 1		
		if (*blank == FB_BLANK_UNBLANK)
			mip_resume(&melfas_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			mip_suspend(&melfas_data->client->dev);
#endif			
	}

	return 0;
}
#endif

/**
* Initialize driver
*/
static int mip_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mip_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;	
	
	//dev_dbg(&client->dev, "%s [START]\n", __func__);
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)){
		//dev_err(&client->dev, "%s [ERROR] i2c_check_functionality\n", __func__);		
		ret = -EIO;
		goto ERROR;
	}

	//Init info data
	info = kzalloc(sizeof(struct mip_ts_info), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!info || !input_dev) {
		//dev_err(&client->dev, "%s [ERROR]\n", __func__);
		ret = -ENOMEM;
		goto ERROR;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->irq = -1;
	info->init = true;
	info->power = -1;
	info->fw_path_ext = kstrdup(FW_PATH_EXTERNAL, GFP_KERNEL);
	
	mutex_init(&info->lock);

	//Get platform data
#if MIP_USE_DEVICETREE
	if (client->dev.of_node) {
		info->pdata = devm_kzalloc(&client->dev, sizeof(struct melfas_platform_data), GFP_KERNEL);
		if (!info->pdata) {
			//dev_err(&client->dev, "%s [ERROR] pdata devm_kzalloc\n", __func__);
			//goto error_platform_data;
		}

		ret = mip_parse_devicetree(&client->dev, info);
		if (ret){
			//dev_err(&client->dev, "%s [ERROR] mip_parse_dt\n", __func__);
			goto ERROR;
		}
	} else
#endif
	{
		info->pdata = client->dev.platform_data;
		if (info->pdata == NULL) {
			//dev_err(&client->dev, "%s [ERROR] pdata is null\n", __func__);
			ret = -EINVAL;
			goto ERROR;
		}
	}

	//Init input device
	info->input_dev->name = MIP_DEVICE_NAME;//"MELFAS_" CHIP_NAME "_Touchscreen";
	snprintf(info->phys, sizeof(info->phys), "%s/input1", info->input_dev->name);
	
	info->input_dev->phys = info->phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->id.vendor= 0x2;
	info->input_dev->id.product= 0x0438;
	info->input_dev->id.version= 0x1508;
	info->input_dev->dev.parent = &client->dev;
	
#if MIP_USE_INPUT_OPEN_CLOSE	
	info->input_dev->open = mip_input_open;
	info->input_dev->close = mip_input_close;
#endif

	//Set info data
	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	//Power on
	mip_power_on(info);

	ret = mip_i2c_test(info);
	if (ret < 0) {
            dev_info(&client->dev, "I2C communication ERROR!\n");
            goto INPUT_REGISTER_DEVICE_ERROR;
	}
	//Firmware update
#if MIP_USE_AUTO_FW_UPDATE
	ret = mip_fw_update_from_kernel(info);
	if(ret){
		//dev_err(&client->dev, "%s [ERROR] mip_fw_update_from_kernel\n", __func__);
	}
#endif
	
	//Initial config
	mip_init_config(info);

#ifdef SYS_ATTR_FILE_MIP4
	sys_fw_ver = info->fw_version;
#endif
	//Config input interface	
	mip_config_input(info);

	//Register input device
	ret = input_register_device(input_dev);
	if (ret) {
		//dev_err(&client->dev, "%s [ERROR] input_register_device\n", __func__);
		ret = -EIO;
		goto INPUT_REGISTER_DEVICE_ERROR;
	}

#if MIP_USE_CALLBACK
	//Config callback functions
	mip_config_callback(info);
#endif

#if MIP_USE_WAKEUP_GESTURE
	//Wake-lock for wake-up gesture mode
	wake_lock_init(&mip_wake_lock, WAKE_LOCK_SUSPEND, "mip_wake_lock");
	device_init_wakeup(&client->dev, 1);
#endif

	//Set interrupt handler	
	ret = request_threaded_irq(client->irq, NULL, mip_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT, MIP_DEVICE_NAME, info);
	//ret = request_threaded_irq(client->irq, NULL, mip_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, MIP_DEVICE_NAME, info);
	if (ret) {
		//dev_err(&client->dev, "%s [ERROR] request_threaded_irq\n", __func__);
		goto IRQ_ERROR;
	}

	disable_irq(client->irq);
	info->irq = client->irq;

#ifdef CONFIG_HAS_EARLYSUSPEND
	//Config early suspend
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +1;
	info->early_suspend.suspend = mip_early_suspend;
	info->early_suspend.resume = mip_late_resume;
	
	register_early_suspend(&info->early_suspend);
	
	//dev_dbg(&client->dev, "%s - register_early_suspend\n", __func__);
#elif defined(CONFIG_FB)
	info->fb_notif.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&info->fb_notif);

	if (ret)
		//dev_err(&client->dev, "Unable to register fb_notifier: %d\n",ret);
#endif

#if MIP_USE_DEV
	//Create dev node (optional)
	if(mip_dev_create(info)){
		//dev_err(&client->dev, "%s [ERROR] mip_dev_create\n", __func__);
		ret = -EAGAIN;
		goto MIP_DEV_ERROR;
	}	

	//Create dev
	info->class = class_create(THIS_MODULE, MIP_DEVICE_NAME);
	device_create(info->class, NULL, info->mip_dev, NULL, MIP_DEVICE_NAME);
#endif

#if MIP_USE_SYS
	//Create sysfs for test mode (optional)
	if (mip_sysfs_create(info)){
		//dev_err(&client->dev, "%s [ERROR] mip_sysfs_create\n", __func__);
		ret = -EAGAIN;
		goto MIP_SYSFS_ERROR;
	}
#endif

#if MIP_USE_CMD
	//Create sysfs for command mode (optional)
	if (mip_sysfs_cmd_create(info)){
		//dev_err(&client->dev, "%s [ERROR] mip_sysfs_cmd_create\n", __func__);
		ret = -EAGAIN;
		goto ERROR;
	}
#endif
	
	//Create sysfs
	if (sysfs_create_group(&client->dev.kobj, &mip_attr_group)) {
		//dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		ret = -EAGAIN;
		goto MIP_ATTR_GROUP_ERROR;
	}

	if (sysfs_create_link(NULL, &client->dev.kobj, MIP_DEVICE_NAME)) {
		//dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		ret = -EAGAIN;
		goto MIP_LINK_ERROR;
	}

	/* add chip info  wangli_20151215 */
#ifdef SYS_ATTR_FILE_MIP4
	add_sysfs_android_touch();
#endif
	/* add chip info  wangli_20151215 */

#if MIP_USE_VIRTUAL_KEYS
	if (info->pdata->create_vkeys)
	{
		ret = mip_VirtualKeysInit(info);
	}
#endif

	//power off device
	mip_power_off(info);

	//dev_dbg(&client->dev, "%s [DONE]\n", __func__);	
	dev_info(&client->dev, "MELFAS " CHIP_NAME " Touchscreen is initialized successfully.\n");
	return 0;

MIP_LINK_ERROR:
	sysfs_remove_group(&client->dev.kobj, &mip_attr_group);
MIP_ATTR_GROUP_ERROR:
#if MIP_USE_SYS
	mip_sysfs_remove(info);
#endif
MIP_SYSFS_ERROR:
#if MIP_USE_DEV
	device_destroy(info->class, info->mip_dev);
	class_destroy(info->class);
#endif
MIP_DEV_ERROR:
IRQ_ERROR:
#if MIP_USE_WAKEUP_GESTURE
	device_init_wakeup(&client->dev, 0);
	wake_lock_destroy(&mip_wake_lock);
#endif
	input_unregister_device(info->input_dev);
INPUT_REGISTER_DEVICE_ERROR:
	mip_power_off(info);

	if (info->regulator_vdd){
		regulator_put(info->regulator_vdd);
		info->regulator_vdd = NULL;
	}
	if (info->regulator_vcc_i2c){
		regulator_put(info->regulator_vcc_i2c);
		info->regulator_vcc_i2c = NULL;
	}

ERROR:
	//dev_dbg(&client->dev, "%s [ERROR]\n", __func__);
	dev_info(&client->dev, "MELFAS " CHIP_NAME " Touchscreen initialization failed.\n");
	return ret;
}

/**
* Remove driver
*/
static int mip_remove(struct i2c_client *client)
{
	struct mip_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0){
		free_irq(info->irq, info);
	}

#if MIP_USE_CMD
	mip_sysfs_cmd_remove(info);
#endif

#if MIP_USE_SYS
	mip_sysfs_remove(info);
#endif

	sysfs_remove_group(&info->client->dev.kobj, &mip_attr_group);
	sysfs_remove_link(NULL, MIP_DEVICE_NAME);
	kfree(info->print_buf);

	/* add chip info  wangli_20151215 */
#ifdef SYS_ATTR_FILE_MIP4
	remove_sysfs_android_touch();
#endif
	/* add chip info  wangli_20151215 */

#if MIP_USE_DEV
	device_destroy(info->class, info->mip_dev);
	class_destroy(info->class);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#elif defined(CONFIG_FB)
	if (fb_unregister_client(&info->fb_notif))
		//dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif

#if MIP_USE_WAKEUP_GESTURE
	device_init_wakeup(&client->dev, 0);
	wake_lock_destroy(&mip_wake_lock);
#endif

	input_unregister_device(info->input_dev);

	mip_power_off(info);
    if (info->regulator_vdd){
        regulator_put(info->regulator_vdd);
        info->regulator_vdd = NULL;
	}
	if (info->regulator_vcc_i2c){
        regulator_put(info->regulator_vcc_i2c);
        info->regulator_vcc_i2c = NULL;
	}

#if MIP_USE_VIRTUAL_KEYS
	if (info->pdata->create_vkeys)
	{
		mip_VirtualKeysUnInit(info);
	}
#endif

	kfree(info->fw_name);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_FB)
/**
* Device suspend event handler
*/
int mip_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	
	//dev_dbg(&client->dev, "%s [START]\n", __func__);
	
	mip_disable(info);
	
	//dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return 0;

}

/**
* Device resume event handler
*/
int mip_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;

	//dev_dbg(&client->dev, "%s [START]\n", __func__);
	mip_clear_input(info);
	mip_enable(info);

	//dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return ret;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
* Early suspend handler
*/
void mip_early_suspend(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);
	
	mip_suspend(&info->client->dev);
}

/**
* Late resume handler
*/
void mip_late_resume(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);

	mip_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
* PM info
*/
const struct dev_pm_ops mip_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend	= mip_suspend,
	.resume = mip_resume,
#endif
};
#endif

#if MIP_USE_DEVICETREE
/**
* Device tree match table
*/
static const struct of_device_id mip_match_table[] = {
	{ .compatible = "melfas,mip4_ts",},
	{},
};
MODULE_DEVICE_TABLE(of, mip_match_table);
#endif

/**
* I2C Device ID
*/
static const struct i2c_device_id mip_id[] = {
	{MIP_DEVICE_NAME, 0},
};
MODULE_DEVICE_TABLE(i2c, mip_id);

/**
* I2C driver info
*/
static struct i2c_driver mip_driver = {
	.id_table = mip_id,
	.probe = mip_probe,
	.remove = mip_remove,
	.driver = {
		.name = MIP_DEVICE_NAME,
		.owner = THIS_MODULE,
#if MIP_USE_DEVICETREE
		.of_match_table = mip_match_table,
#endif
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm 	= &mip_pm_ops,
#endif
	},
};

/**
* Init driver
*/
static int __init mip_init(void)
{
	return i2c_add_driver(&mip_driver);
}

/**
* Exit driver
*/
static void __exit mip_exit(void)
{
	i2c_del_driver(&mip_driver);
}

module_init(mip_init);
module_exit(mip_exit);

MODULE_DESCRIPTION("MELFAS MIP4 Touchscreen");
MODULE_VERSION("2015.07.20");
MODULE_AUTHOR("Jee, SangWon <jeesw@melfas.com>");
MODULE_LICENSE("GPL"); 
