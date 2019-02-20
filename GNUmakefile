# Maintainer's make file for dico.
# 
ifneq (,$(wildcard Makefile))
 include Makefile
configure.ac: configure.boot
	./bootstrap
else
$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all):
	$(MAKE) -f maint/bootstrap.mk
	$(MAKE) $(MAKECMDGOALS)
endif
