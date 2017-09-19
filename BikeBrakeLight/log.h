/*
 * log.h
 *
 * Created: 13/08/2017 22:49:19
 *  Author: Spikey
 */ 


#ifndef LOG_H_
#define LOG_H_

void LOGEventHandler(Event event, U32 eventArg);
void LOGprintf(char* str, ...);
void LOGputc(char item);

#endif /* LOG_H_ */