STB_PATH = $(HOME)/stb

CFLAGS = -std=c++17 -O2 -I$(STB_PATH)
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

VulkanTest: main.cpp
	g++ $(CFLAGS) -o VulkanTest main.cpp $(LDFLAGS) -g -Wall
default:
	VulkanTest

.PHONY: test clean

Shaders:
	cd shaders && glslc shader.vert -o vert.spv
	cd shaders && glslc shader.frag -o frag.spv

test: Shaders VulkanTest
	./VulkanTest
	
clean:
	rm -f VulkanTest
	rm -f .makefile.*
	rm -f *spv