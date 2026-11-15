# Included after the pinned Our64 player Makefile, from its player directory.
# Only machine, graphics, input and save services enter this archive. The
# Our64 application entry point, library, settings, artwork and UI stay out.
N64_ENGINE_OUT ?= $(CURDIR)/../../.cores/n64/not64_gliden64_wiiu.a

N64_ENGINE_BENCH := our64_jit lwp_heap romcache
N64_ENGINE_PLAYER := plugins audio saves saves_wiiu states input \
	writer gamedb tpak gb_tower_hle
N64_ENGINE_OBJECTS := \
	$(addprefix $(BUILD)/not64/,$(NOT64_SRC:.c=.o)) \
	$(addprefix $(BUILD)/bench/,$(addsuffix .o,$(N64_ENGINE_BENCH))) \
	$(addprefix $(BUILD)/source/,$(addsuffix .o,$(N64_ENGINE_PLAYER))) \
	$(BUILD)/n64-retrou-video.o \
	$(BUILD)/n64-retrou-gliden64-common.o $(BUILD)/n64-retrou-gliden64-window.o \
	$(BUILD)/minilzo.o $(BUILD)/cxd4.o $(BUILD)/replay/core_wiiu.o \
	$(filter-out $(BUILD)/gliden64/common/CommonAPIImpl_common.o $(BUILD)/gliden64/DisplayWindow.o, \
	$(addprefix $(BUILD)/gliden64/,$(GLIDEN64_CORE:.cpp=.o) $(GLIDEN64_C:.c=.o)))

.PHONY: n64_engine

$(BUILD)/n64-retrou-video.o: $(N64_ENGINE_PATCHED_VIDEO)
	@mkdir -p $(dir $@)
	@echo n64-retrou-video.cpp
	@$(CXX) -MMD -MP $(CXXFLAGS) -I$(CURDIR)/source -Wall -c $< -o $@
$(BUILD)/n64-retrou-gliden64-common.o: $(N64_ENGINE_PATCHED_GLIDEN64_COMMON)
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP $(CXXFLAGS) -c $< -o $@
$(BUILD)/n64-retrou-gliden64-window.o: $(N64_ENGINE_PATCHED_GLIDEN64_WINDOW)
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP $(CXXFLAGS) -c $< -o $@
n64_engine: $(N64_ENGINE_OUT)

$(N64_ENGINE_OUT): $(N64_ENGINE_OBJECTS)
	@mkdir -p $(dir $@)
	@rm -f $@
	@$(AR) rcs $@ $^
	@echo built ... $@
