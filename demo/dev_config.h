// Last Update:2018-11-27 10:51:16
/**
 * @file dev_config.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-07-05
 */

/*
 * dev core - manage all the registered capture devices
 * if one capture device need to be enabled
 * it should be added to dev_config.h with macro 
 * DEV_CORE_CAPTURE_DEVICE_ENTRY
 * */
#ifdef ENABLE_IPC
DEV_CORE_CAPTURE_DEVICE_ENTRY( gIpcCaptureDev )
#else
DEV_CORE_CAPTURE_DEVICE_ENTRY( gSimDevCaptureDev )
#endif


