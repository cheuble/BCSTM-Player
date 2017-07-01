#include <string.h>
#include <3ds.h>
#include <stdio.h>
#include "MusicBCSTM.hpp"
int main(int argc, char ** argv){
	gfxInitDefault();
	ndspInit();
	consoleInit(GFX_TOP, NULL);
	MusicBCSTM m;
	m.openFromFile("sdmc:/music.bcstm");
	m.play();
	while(aptMainLoop()){
		m.tick();
	}
	m.stop();
	gfxExit();
	ndspExit();
	aptExit();
	return 0;
}