
INCLUDES = \
	$(CLUTTER_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Colord\"

noinst_PROGRAMS =						\
	cd-glsl-demo

cd_glsl_demo_SOURCES =						\
	cd-glsl-demo.c

cd_glsl_demo_LDADD =						\
	$(CLUTTER_LIBS)
	$(lib_LTLIBRARIES)

EXTRA_DIST = image-widget-good.png

-include $(top_srcdir)/git.mk
