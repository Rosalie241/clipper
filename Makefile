clipper_exe:
	gcc -g clipper/clipper.cpp \
		3rdParty/hidapi/linux/hid.c -I3rdParty/hidapi/hidapi -ludev \
		-I/usr/include/libevdev-1.0 -levdev \
		-I3rdParty/ViGEmClient-Linux/include \
		3rdParty/ViGEmClient-Linux/src/ViGEmClient.cpp