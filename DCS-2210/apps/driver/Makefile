ROOTDIR = ../..
include $(ROOTDIR)/Rules.make

	KMODS = osd gpio ioport

all:
	@for i in $(KMODS) ; do make -C $$i $@ ; done

install:
	@for i in $(KMODS) ; do make -C $$i $@ ; done

clean:
	-for i in $(KMODS) ; do make -C $$i clean ; done

cleanall:
	-for i in $(KMODS) ; do make -C $$i clean ; done

