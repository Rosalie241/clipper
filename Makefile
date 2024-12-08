clipper_exe:
	g++ -O3 -flto clipper/clipper.cpp clipper/guitar.cpp clipper/drum.cpp \
		3rdParty/hidapi/linux/hid.c -I3rdParty/hidapi/hidapi -ludev \
		-I/usr/include/libevdev-1.0 -levdev \
		-I3rdParty/ViGEmClient-Linux/include \
		-I3rdParty/inih/cpp \
		3rdParty/ViGEmClient-Linux/src/ViGEmClient.cpp \
		3rdParty/inih/ini.c \
		3rdParty/inih/cpp/INIReader.cpp
