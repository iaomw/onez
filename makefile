CFLAGS = -std=c++17 -O2 -I $(VULKAN_SDK)/include -I .

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)        # Mac OS X
	LDFLAGS = -L$(VULKAN_SDK)/lib `pkg-config --static --libs glfw3` -lvulkan
else
	LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
endif

# ifeq ($(detected_OS),Linux)
# endif

default: shaders vktest

vktest: main.cpp
	g++ $(CFLAGS) -o vktest main.cpp $(LDFLAGS) -g -Wall

.PHONY: shaders test clean

shaders:
	cd shaders && glslc shader.vert -o vert.spv
	cd shaders && glslc shader.frag -o frag.spv

test: default
	./vktest
	
clean:
	rm -f vktest
	rm -f makefile.*
	cd shaders && rm -f *.spv