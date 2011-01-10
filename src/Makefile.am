AM_CPPFLAGS = -m64 -DNDEBUG -I$(srcdir)/cmph_0_9 -I$(srcdir)/zlib-1.2.3 -I$(top_srcdir)/boost_1_42_0

SUBDIRS = cmph_0_9 zlib-1.2.3

bin_PROGRAMS = shefLMStore

shefLMStore_SOURCES = macros.h CompactStore.h CompressedValueStoreElias.h CompressedValueStoreFibonacci.h CompressedValueStore.h MPHR.h ShefBitArray.h KneserNeyWrapper.h FingerPrintStore.h simple_select11.h simple_select_half.h rank9.h rank9sel.h simple_select.h simple_select_zero_half.h elias_fano.h zlib.cpp gzip.cpp rank9.cpp rank9sel.cpp elias_fano.cpp simple_select_half.cpp simple_select11.cpp simple_select_zero_half.cpp simple_select.cpp archive_exception.cpp basic_archive.cpp basic_iarchive.cpp basic_iserializer.cpp basic_oarchive.cpp basic_oserializer.cpp basic_pointer_iserializer.cpp basic_pointer_oserializer.cpp basic_serializer_map.cpp basic_text_iprimitive.cpp basic_text_oprimitive.cpp basic_text_wiprimitive.cpp basic_text_woprimitive.cpp binary_iarchive.cpp binary_oarchive.cpp binary_wiarchive.cpp binary_woarchive.cpp utf8_codecvt_facet.cpp codecvt_null.cpp extended_type_info.cpp extended_type_info_no_rtti.cpp extended_type_info_typeid.cpp shared_ptr_helper.cpp stl_port.cpp text_iarchive.cpp text_oarchive.cpp text_wiarchive.cpp text_woarchive.cpp void_cast.cpp main.cpp

shefLMStore_LDADD = cmph_0_9/libcmph.a zlib-1.2.3/libzlib.a



