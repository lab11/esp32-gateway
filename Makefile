#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp32-gateway

include $(IDF_PATH)/make/project.mk

all: | $(shell for file in main/www/*; do xxd -i $$file; done > main/www.h)