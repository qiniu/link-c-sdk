// Last Update:2019-03-21 20:44:47
/**
 * @file dbg.h
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-21
 */

#ifndef DBG_H
#define DBG_H

#include <stdio.h>

#define RED                  "\e[0;31m"
#define NONE                 "\e[0m"
#define GREEN                "\e[0;32m"
#define BLUE                 "\e[0;34m"

#define BASIC() printf("[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define LOGI(args...) BASIC();printf(args)
#define LOGE(args...) printf(RED"[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ );printf(args);printf(NONE)
#define LOGW(args...) LOGI(args)
#define LOGT(args...) printf(GREEN"[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ );printf(args);printf(NONE)
#define LOGD(args...) printf(BLUE"[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ );printf(args);printf(NONE)


#endif  /*DBG_H*/
