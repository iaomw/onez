CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

VulkanTest: main.cpp
	g++ $(CFLAGS) -o VulkanTest main.cpp $(LDFLAGS) -g -Wall

.PHONY: test clean

Shaders:
	glslc ./shaders/shader.vert -o vert.spv
	glslc ./shaders/shader.frag -o frag.spv

test: Shaders VulkanTest
	./VulkanTest
	
clean:
	rm -f VulkanTest
	rm -f .makefile.*
	rm -f *spv