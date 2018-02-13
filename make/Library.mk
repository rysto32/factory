
ifneq ($(LIB),)

define src_to_obj
$(addprefix $($(LIB)_OBJDIR)/, $(addsuffix .o,$(basename $1)))
endef

define src_to_dep
$(addprefix $1,$(addsuffix .d,$(notdir $2)))
endef

$(LIB)_SRCDIR := $(CURDIR)
$(LIB)_OBJDIR := $(OBJDIR)/$(CURDIR)
$(LIB)_DOBJS := $(call src_to_obj,$(SRCS))
$(LIB)_SOBJS := $(addsuffix .So,$(basename $($(LIB)_DOBJS)))
$(LIB)_LIBRARY := $(LIBDIR)/lib$(LIB).a
$(LIB)_DEPDIR := $(DEPENDDIR)/$(CURDIR)
$(LIB)_SDEPDIR := $(SDEPENDDIR)/$(CURDIR)
DEPFILES := $(call src_to_dep,$($(LIB)_DEPDIR)/,$(SRCS))
$(LIB)_CLEAN := $($(LIB)_DOBJS) $($(LIB)_LIBRARY)
$(LIB)_STDLIBS := $(addprefix -l,$(SHLIB_STDLIBS))
SHLIB_VERSION ?= 1

$($(LIB)_LIBRARY): $($(LIB)_DOBJS)
	mkdir -p $(dir $@)
	rm -f $@
	$(AR) -crs $@ $^

$($(LIB)_DOBJS): LOCAL_INCLUDE := $(LOCAL_INCLUDE)
$($(LIB)_DOBJS): LIB := $(LIB)

$($(LIB)_SOBJS): LOCAL_INCLUDE := $(LOCAL_INCLUDE)
$($(LIB)_SOBJS): LIB := $(LIB)

$(OBJDIR)/%.So: %.c
	mkdir -p $(dir $@) $($(LIB)_SDEPDIR)
	$(CC) -MD -MF $(call src_to_dep,$($(LIB)_SDEPDIR)/,$<) -fPIC -c $(CFLAGS) $(LOCAL_INCLUDE) $< -o $@

$(OBJDIR)/%.o: %.c
	mkdir -p $(dir $@) $($(LIB)_DEPDIR)
	$(CC) -MD -MF $(call src_to_dep,$($(LIB)_DEPDIR)/,$<) -c $(CFLAGS) $(LOCAL_INCLUDE) $< -o $@

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@) $($(LIB)_DEPDIR)
	$(CXX) -MD -MF $(call src_to_dep,$($(LIB)_DEPDIR)/,$<) -c $(CFLAGS) $(CXXFLAGS) $(LOCAL_INCLUDE) $< -o $@

ifneq ($(SHLIB_NEEDED),)

$(LIB)_SONAME := lib$(LIB).so.$(SHLIB_VERSION)
$(LIB)_SHLIB := $(LIBDIR)/$($(LIB)_SONAME)

$(LIB)_CLEAN := $($(LIB)_CLEAN) $($(LIB)_SHLIB) $($(LIB)_SOBJS)

$($(LIB)_SHLIB): $($(LIB)_SOBJS)
	mkdir -p $(dir $@)
	$(LD_C) -shared $(LDFLAGS) -o $@ -soname $($(LIB)_SONAME) $^ $($(LIB)_STDLIBS)

libraries:: $($(LIB)_SHLIB)


DEPFILES := $(DEPFILES) $(call src_to_dep,$($(LIB)_SDEPDIR)/,$(SRCS))

endif

$(LIB)_CLEAN := $($(LIB)_CLEAN) $(DEPFILES)


-include $(DEPFILES)

.PHONY: clean_lib_$(LIB)

clean:: clean_lib_$(LIB)

clean_lib_$(LIB): LIB:=$(LIB)

clean_lib_$(LIB):
	$(RM) $($(LIB)_CLEAN)

endif