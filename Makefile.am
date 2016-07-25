SUBDIRS = src

# config file
confdir = $(sysconfdir)
dist_conf_DATA = iaq-measurementd.cfg

# documentation
dist_doc_DATA = README.md COPYING

EXTRA_DIST = licenses/RASPI_CONFIG_LICENSE licenses/CHROMIUM_LICENSE

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(pkgstatedir)

uninstall-local:
	rm -r $(DESTDIR)$(pkgstatedir)
