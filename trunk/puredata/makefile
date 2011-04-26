all:
	gcc `libusb-config --cflags` -c uDMX.c -o uDMX.o 
	gcc -bundle -undefined suppress -flat_namespace -o uDMX.pd_darwin uDMX.o `libusb-config --libs` -framework CoreFoundation --enable-fat-binary=i386
	mv uDMX.pd_darwin ../uDMX.pd_darwin
	
clean:
	rm *.o
