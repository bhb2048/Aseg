#
#	Makefile for Aseg/LevelSet
#	
#	16-sep-11
#
#	Modified:
#	22-apr-14	use src/makeinclude
#	
include ../../makeinclude
include ../makeinclude

SOURCES = 	SegParameters.cc

LIBOBJS = $(SOURCES:.cc=.o)

%.o : %.cc
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(LIB):	$(LIB)($(LIBOBJS))

clean:
	rm -f $(LIB) *.o

tidy:
	rm -f *.o

depend:
	makedepend -I. -I/usr/local/include $(SOURCES) >/dev/null 2>&1
# DO NOT DELETE

SegParameters.o: SegParameters.h /usr/local/include/ITK/Common.h
SegParameters.o: ../UI/SegParametersUI.h /usr/local/include/fltk/Window.h
SegParameters.o: /usr/local/include/fltk/Group.h
SegParameters.o: /usr/local/include/fltk/Widget.h
SegParameters.o: /usr/local/include/fltk/Style.h
SegParameters.o: /usr/local/include/fltk/Color.h
SegParameters.o: /usr/local/include/fltk/FL_API.h
SegParameters.o: /usr/local/include/fltk/Flags.h
SegParameters.o: /usr/local/include/fltk/Rectangle.h
SegParameters.o: /usr/local/include/Fltk/Dialog.h
SegParameters.o: /usr/local/include/fltk/Group.h
SegParameters.o: /usr/local/include/fltk/Button.h
SegParameters.o: /usr/local/include/fltk/Widget.h /usr/include/stdio.h
SegParameters.o: /usr/include/features.h /usr/include/stdc-predef.h
SegParameters.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
SegParameters.o: /usr/include/gnu/stubs.h /usr/include/bits/types.h
SegParameters.o: /usr/include/bits/typesizes.h /usr/include/libio.h
SegParameters.o: /usr/include/_G_config.h /usr/include/wchar.h
SegParameters.o: /usr/include/bits/stdio_lim.h
SegParameters.o: /usr/include/bits/sys_errlist.h
SegParameters.o: /usr/local/include/fltk/file_chooser.h /usr/include/string.h
SegParameters.o: /usr/include/xlocale.h /usr/local/include/fltk/TabGroup.h
SegParameters.o: /usr/local/include/fltk/ValueInput.h
SegParameters.o: /usr/local/include/fltk/Valuator.h
SegParameters.o: /usr/local/include/fltk/FloatInput.h
SegParameters.o: /usr/local/include/fltk/NumericInput.h
SegParameters.o: /usr/local/include/fltk/Input.h
SegParameters.o: /usr/local/include/fltk/Input.h
SegParameters.o: /usr/local/include/fltk/FileChooser.h
SegParameters.o: /usr/local/include/fltk/DoubleBufferWindow.h
SegParameters.o: /usr/local/include/fltk/Choice.h
SegParameters.o: /usr/local/include/fltk/Menu.h
SegParameters.o: /usr/local/include/fltk/PopupMenu.h
SegParameters.o: /usr/local/include/fltk/Preferences.h
SegParameters.o: /usr/local/include/fltk/TiledGroup.h
SegParameters.o: /usr/local/include/fltk/FileBrowser.h
SegParameters.o: /usr/local/include/fltk/Browser.h
SegParameters.o: /usr/local/include/fltk/Scrollbar.h
SegParameters.o: /usr/local/include/fltk/Slider.h
SegParameters.o: /usr/local/include/fltk/FileIcon.h
SegParameters.o: /usr/local/include/fltk/Symbol.h
SegParameters.o: /usr/local/include/fltk/PixelType.h
SegParameters.o: /usr/local/include/fltk/filename.h /usr/include/sys/types.h
SegParameters.o: /usr/include/time.h /usr/include/endian.h
SegParameters.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
SegParameters.o: /usr/include/bits/byteswap-16.h /usr/include/sys/select.h
SegParameters.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
SegParameters.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
SegParameters.o: /usr/include/bits/pthreadtypes.h /usr/include/sys/stat.h
SegParameters.o: /usr/include/bits/stat.h /usr/include/dirent.h
SegParameters.o: /usr/include/bits/dirent.h /usr/include/bits/posix1_lim.h
SegParameters.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
SegParameters.o: /usr/local/include/fltk/InvisibleBox.h
SegParameters.o: /usr/local/include/fltk/CheckButton.h
SegParameters.o: /usr/local/include/fltk/Button.h
SegParameters.o: /usr/local/include/fltk/FileInput.h
SegParameters.o: /usr/local/include/fltk/ReturnButton.h
SegParameters.o: /usr/local/include/fltk/ask.h /usr/local/include/fltk/run.h
