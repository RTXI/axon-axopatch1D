PLUGIN_NAME = axon_axopatch1D

HEADERS = axon-axopatch1D.h 

SOURCES = axon-axopatch1D.cpp \
          moc_axon-axopatch1D.cpp 

LIBS = 

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
