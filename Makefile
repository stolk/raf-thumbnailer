CFLAGS = -g -Wall

LDFLAGS += -lm

TARGET = raf-thumbnailer

all: $(TARGET)

run: $(TARGET)
	./raf-thumbnailer IMAGE.RAF IMAGE.PNG

$(TARGET): raf-thumbnailer.c
	$(CC) $(CFLAGS) raf-thumbnailer.c $(LDFLAGS) -o $(TARGET)


DISTFILES=\
Makefile \
raf-thumbnailer.c \
raf-thumbnailer.1 \
stb_image.h \
stb_image_write.h \
README.md \
LICENSE \
thumbnailers \
debian

clean:
	$(RM) $(TARGET)
	$(RM) -r debian/raf-thumbnailer
	$(RM) -r debian/.debhelper
	$(RM) debian/debhelper-buildstamp
	$(RM) debian/raf-thumbnailer.substvars
	$(RM) debian/files

install: raf-thumbnailer
	install -d ${DESTDIR}/usr/bin
	install -m 755 raf-thumbnailer ${DESTDIR}/usr/bin/
	install -d ${DESTDIR}/usr/share/thumbnailers
	install -m 755 thumbnailers/raf.thumbnailer ${DESTDIR}/usr/share/thumbnailers/

uninstall:
	rm -f ${DESTDIR}/usr/bin/raf-thumbnailer

tarball:
	rm -f ../raf-thumbnailer_*
	tar cvzf ../raf-thumbnailer_1.0.orig.tar.gz $(DISTFILES)

packageupload:
	debuild -S
	debsign ../raf-thumbnailer_1.0-1_source.changes
	dput ppa:b-stolk/ppa ../raf-thumbnailer_1.0-1_source.changes

