AM_CPPFLAGS = -Wall \
             -g -O0 \
             $(DIAG_CFLAGS) \
             -I$(srcdir) \
             -I$(srcdir)/vendor_nan \
             -I$(srcdir)/wifi_hal_ctrl

if DEBUG
AM_CPPFLAGS += DDEBUG
endif

requiredlibs = -lrt $(DIAG_LIBS)

AM_CPPFLAGS += $(CPPFLAGS)

if USE_GLIB
AM_CFLAGS = -DUSE_GLIB $(GLIB_CFLAGS)
AM_CPPFLAGS += -DUSE_GLIB $(GLIB_CFLAGS)
requiredlibs += $(GLIB_LIBS)
endif

h_sources = ${WORKSPACE}/system/core/include/log/log.h

#include sub directories header files
h_sources += $(srcdir)/vendor_nan/vendor_nan_hal.h
h_sources += $(srcdir)/wifi_hal_ctrl/wifi_hal_ctrl.h

c_sources = list.cpp
c_sources += nan.cpp
c_sources += wifi_hal.cpp
c_sources += common.cpp
c_sources += cpp_bindings.cpp
c_sources += llstats.cpp
c_sources += gscan.cpp
c_sources += gscan_event_handler.cpp
c_sources += rtt.cpp
c_sources += ifaceeventhandler.cpp
c_sources += tdls.cpp
c_sources += nan_ind.cpp
c_sources += nan_req.cpp
c_sources += nan_rsp.cpp
c_sources += wificonfig.cpp
c_sources += wifilogger.cpp
c_sources += wifilogger_diag.cpp
c_sources += ring_buffer.cpp
c_sources += rb_wrapper.cpp
c_sources += rssi_monitor.cpp
c_sources += roam.cpp
c_sources += radio_mode.cpp
c_sources += wifihal_vendor.cpp
c_sources += nan_pairing.cpp
c_sources += nan_pairing_initiator.cpp
c_sources += nan_pairing_responder.cpp
c_sources += tcp_params_update.cpp
c_sources += twt.cpp
c_sources += wifi_cached_scan_result.cpp

#include sub-directories source files
c_sources += $(srcdir)/vendor_nan/vendor_nan.cpp
c_sources += $(srcdir)/wifi_hal_ctrl/wifi_hal_ctrl.c

library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)

libwifihalqcom_la_SOURCES = $(c_sources) $(h_sources)
libwifihalqcom_la_LIBADD = ${requiredlibs}
libwifihalqcom_la_CPPFLAGS =$(AM_CPPFLAGS)
lib_LTLIBRARIES = libwifihalqcom.la
