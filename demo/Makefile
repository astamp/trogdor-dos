WCC=wcc
#CCFLAGS+=-0 -fpc -wx -zm -mc -d2
# CCFLAGS=-mc -d2 -zm -2 -ontx
CCFLAGS=-q -mc -zm -2 -ontx -wx

# Delete built files on error.
.ERASE

WLINK=wlink
LDFLAGS=debug dwarf system dos option eliminate

.EXTENSIONS:
.EXTENSIONS: .com .exe .obj .asm .c .cpp .h

all: octo.exe

GFXS=GUPA01.DAT GUPB01.DAT GUPC01.DAT GUPD01.DAT GUPE01.DAT GUPX01.DAT LOGO01.DAT GRUBER01.DAT BARROT03.DAT CODISH02.DAT PIKATO01.DAT TOMINO03.DAT TUNIP01.DAT
gfxauto.c: $(GFXS)
	DEL gfxauto.c
	for %i in ($<) do xxd -i %i >> $^@

.c.obj: .AUTODEPEND
	wcc $[* $(CCFLAGS)

OBJS=main.obj video.obj ring.obj chooser.obj gfx.obj k_int9h.obj debug.obj gfxauto.obj coin2.obj collide.obj sound.obj vegresq.obj vbattle.obj

octo.exe: $(OBJS)
	*$(WLINK) $(LDFLAGS) name $^@ file { $< }

clean: .SYMBOLIC
	DEL GFXAUTO.C
	DEL *.OBJ
	DEL *.ERR
	DEL *.EXE
	DEL *.COM
	DEL *.MAP

