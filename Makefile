PLUGIN_NAME = axon_axopatch1D

HEADERS = axon_axopatch1d_commander.h \
          axon_axopatch1d_commanderUI.h

SOURCES = axon_axopatch1d_commander.cpp \
          axon_axopatch1d_commanderUI.cpp \
			 moc_axon_axopatch1d_commander.cpp \
			 moc_axon_axopatch1d_commanderUI.cpp

LIBS = 

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
