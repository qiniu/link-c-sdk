// Last Update:2019-01-30 11:59:31
/**
 * @file log2mqtt.h
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-01-30
 */

#ifndef LOG2MQTT_H
#define LOG2MQTT_H

extern int MqttInit( char *_pClientId, int qos, char *_pUserName,
              char *_pPasswd, char *_pTopic, char *_pHost,
              int _nPort);
extern int LogOverMQTT( char *msg );

#endif  /*LOG2MQTT_H*/
