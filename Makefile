.PHONY: all

build-firmware:
	pio run -e cyd

build-filesystem:
	pio run -e cyd -t buildfs

build: build-firmware build-filesystem

upload-firmware:
	pio run -e cyd -t upload

upload-fs:
	pio run -e cyd -t uploadfs

upload: upload-firmware upload-fs

deploy: build upload

clean:
	pio run -t clean

monitor:
	pio device monitor

all: deploy monitor
