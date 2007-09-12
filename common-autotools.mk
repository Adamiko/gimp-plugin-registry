
SRCDIR = ./src


PREFIX = /usr
MANDIR = $(PREFIX)/share/man
INFODIR = $(PREFIX)/share/info
CONFFLAGS += --prefix=$(PREFIX) --mandir=$(MANDIR) --infodir=$(INFODIR)

DEB_HOST_GNU_TYPE   ?= $(shell dpkg-architecture -qDEB_HOST_GNU_TYPE)
DEB_BUILD_GNU_TYPE  ?= $(shell dpkg-architecture -qDEB_BUILD_GNU_TYPE)
ifeq ($(DEB_BUILD_GNU_TYPE), $(DEB_HOST_GNU_TYPE))
  CONFFLAGS += --build $(DEB_HOST_GNU_TYPE)
else
  CONFFLAGS += --build $(DEB_BUILD_GNU_TYPE) --host $(DEB_HOST_GNU_TYPE)
endif


$(SRCDIR)/config.status: $(SRCDIR)/configure
ifneq "$(wildcard /usr/share/misc/config.sub)" ""
	cp -f /usr/share/misc/config.sub $(SRCDIR)/config.sub
endif
ifneq "$(wildcard /usr/share/misc/config.guess)" ""
	cp -f /usr/share/misc/config.guess $(SRCDIR)/config.guess
endif
	cd $(SRCDIR); ./configure $(CONFFLAGS) \
                                  CFLAGS="$(CFLAGS)" \
                                  LDFLAGS="$(LDFLAGS)" \
                                  CPPFLAGS="$(CPPFLAGS)"

build: build-stamp

build-stamp: $(SRCDIR)/config.status
	cd $(SRCDIR); make
	touch $@

install: build-stamp
	cd $(SRCDIR); make install DESTDIR=$(DESTDIR)

clean:
	rm -f build-stamp
	cd $(SRCDIR); [ ! -r Makefile ] || make distclean
	rm -f $(SRCDIR)/config.sub $(SRCDIR)/config.guess


.PHONY: install clean build

