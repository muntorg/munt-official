ifneq ($(host),$(build))
host_toolchain:=$(host)-
endif

ifeq ($(CC),)
  default_host_CC = $(host_toolchain)gcc
else
  default_host_CC= $(CC)
endif

ifeq ($(CXX),)
  default_host_CXX = $(host_toolchain)g++
else
  default_host_CXX = $(CXX)
endif

ifeq ($(AR),)
  default_host_AR = $(host_toolchain)ar
  ifeq (, $(shell which $(default_host_AR)))
     default_host_AR = ar
  endif
else
  default_host_AR = $(AR)
endif

ifeq ($(RANLIB),)
  default_host_RANLIB = $(host_toolchain)ranlib
  ifeq (, $(shell which $(default_host_RANLIB)))
     default_host_RANLIB = ranlib
  endif
else
  default_host_RANLIB = $(RANLIB)
endif

ifeq ($(STRIP),)
  default_host_STRIP = $(host_toolchain)strip
  ifeq (, $(shell which $(default_host_STRIP)))
     default_host_STRIP = strip
  endif
else
  default_host_STRIP = $(RANLIB)
endif

ifeq ($(LIBTOOL),)
  default_host_LIBTOOL = $(host_toolchain)libtool
  ifeq (, $(shell which $(default_host_LIBTOOL)))
     default_host_LIBTOOL = libtool
  endif
else
  default_host_LIBTOOL = $(LIBTOOL)
endif

ifeq ($(NM),)
  default_host_NM = $(host_toolchain)nm
  ifeq (, $(shell which $(default_host_NM)))
     default_host_NM = nm
  endif
else
  default_host_NM = $(NM)
endif

default_host_INSTALL_NAME_TOOL = $(host_toolchain)install_name_tool
default_host_OTOOL = $(host_toolchain)otool

define add_host_tool_func
$(host_os)_$1?=$$(default_host_$1)
$(host_arch)_$(host_os)_$1?=$$($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1?=$$($(host_os)_$1)
host_$1=$$($(host_arch)_$(host_os)_$1)
endef

define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 += $($(host_os)_$(release_type)_$1)
host_$1 = $$($(host_arch)_$(host_os)_$1)
host_$(release_type)_$1 = $$($(host_arch)_$(host_os)_$(release_type)_$1)
endef

$(foreach tool,CC CXX AR RANLIB STRIP NM LIBTOOL OTOOL INSTALL_NAME_TOOL,$(eval $(call add_host_tool_func,$(tool))))
$(foreach flags,CFLAGS CXXFLAGS CPPFLAGS LDFLAGS, $(eval $(call add_host_flags_func,$(flags))))
