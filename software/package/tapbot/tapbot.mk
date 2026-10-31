################################################################################
#
# tapbot
#
################################################################################

TAPBOT_VERSION = 0.1
TAPBOT_SITE = $(TAPBOT_PKGDIR)/src
TAPBOT_SITE_METHOD = local
TAPBOT_LICENSE = MIT

# clean first: binaries left in src/ by a host 'make test' are rsynced in
# with the sources and would otherwise be installed as they are (x86)
define TAPBOT_BUILD_CMDS
	$(MAKE) -C $(@D) clean
	$(TARGET_MAKE_ENV) $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define TAPBOT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/tmc2209ctl $(TARGET_DIR)/usr/bin/tmc2209ctl
	$(INSTALL) -D -m 0755 $(@D)/tapbot $(TARGET_DIR)/usr/bin/tapbot
	$(INSTALL) -D -m 0644 $(TAPBOT_PKGDIR)/tapbot.conf \
		$(TARGET_DIR)/etc/tapbot.conf
endef

define TAPBOT_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(TAPBOT_PKGDIR)/S95tapbot \
		$(TARGET_DIR)/etc/init.d/S95tapbot
endef

$(eval $(generic-package))
