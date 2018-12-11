
PROGRAM	:= prog

# Must be the first rule
.PHONY: default
default: $(PROGRAM)

ARCH := $(shell uname -s | tr a-z A-Z)

SOURCES := $(wildcard src/*.c)

OBJECTS := $(SOURCES:src/%.c=obj/%.o)
DEPENDS := $(SOURCES:src/%.c=deps/%.d)

MAKEFILES := Makefile

CFLAGS += -O3 -Wall -Wno-unused-function #-march=native
CFLAGS += -I./include

CC=gcc

CFLAGS += -fopenmp
LDFLAGS += -fopenmp

CFLAGS += -DCL_SILENCE_DEPRECATION

#CFLAGS += -DENABLE_VECTO -DVEC_SIZE=8 -mavx2 -mfma
#CFLAGS += -DENABLE_VECTO -DVEC_SIZE=4 -msse4 -mfma

CFLAGS += $(shell pkg-config SDL2_image --cflags)
LDLIBS += $(shell pkg-config SDL2_image --libs)

CFLAGS += $(shell pkg-config --cflags hwloc)
LDLIBS += $(shell pkg-config --libs hwloc)


ifeq ($(ARCH),DARWIN)
LDLIBS		+= -framework OpenCL -framework OpenGL
else
CFLAGS		+= -rdynamic
LDFLAGS		+= -export-dynamic
LDLIBS		+= -lOpenCL -lGL -lpthread -ldl
endif

$(OBJECTS): $(MAKEFILES)

$(PROGRAM): $(OBJECTS) #$(LIB)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(OBJECTS): obj/%.o: src/%.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: depend
depend: $(DEPENDS)

$(DEPENDS): $(MAKEFILES)

$(DEPENDS): deps/%.d: src/%.c
	$(CC) $(CFLAGS) -MM $< | \
		sed -e 's|\(.*\)\.o:|deps/\1.d obj/\1.o:|g' > $@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDS)
endif

.PHONY: clean
clean: 
	rm -f prog obj/*.o deps/*.d lib/*.a
