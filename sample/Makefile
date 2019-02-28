######################################
# Makefile for sample
# copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.
######################################
#source file
SOURCE  := $(wildcard *.c)
OBJS    := $(patsubst %.c,%.o,$(SOURCE))

#target you can change test to what you want
TARGET  := sample

#compile and lib parameter
LIBS    := -ltsuploader -lmqtt -lwolfmqtt -lpthread -lrt -lm
LDFLAGS := -L../lib
DEFINES := -DSAMPLE
INCLUDE := -I../include
CFLAGS  := -g -Wall -O3 $(DEFINES) $(INCLUDE)

#i think you should do anything here
.PHONY : objs clean distclean rebuild

all : $(TARGET)

objs : $(OBJS)

rebuild: distclean all

clean :
	rm -fr *.o

distclean : clean
	rm -fr $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)