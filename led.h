#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sys/time.h>

#include <Magick++.h>
#include <magick/image.h>

#include "led-matrix.h"
#include "graphics.h"
#include "content-streamer.h"

#include "util.h"

using namespace std;
using namespace rgb_matrix;
using namespace util;

typedef int64_t tmillis_t;
struct ImageParams;
struct FileInfo;

class Led {
	public:
		static Led* getInstance();
		//Needed for singletons
		Led(Led const&) = delete;
		void operator=(Led const&) = delete;
		~Led();
		
		void printText(string text);
        void printPicture(string data);
        void cancelAction();
		
	private:
		static Led* instance;
		Led();
		static void setUpInterruptHandler();
		static void interrupt(int);
        
        static void showText(Led *led, string text);
        static void showPicture(Led *led, string data);
        
        void calculateColor(int *continuum, int *red, int *green, int *blue);
        void prepareThread(void (*func)(Led *, string), string text);
        
        static void readImageFromBuffer(string data, std::vector<Magick::Image> *result);
        static void storeInStream(const Magick::Image &img, int delay_time_us, FrameCanvas *scratch, StreamWriter *output);
        static void displayAnimation(const FileInfo *fileInfo, FrameCanvas *offscreen_canvas);
        static tmillis_t getTimeInMillis();
        static void sleepMillis(tmillis_t milli_seconds);
        
        RGBMatrix *matrix;
        Font font;
        Color color;
        
        thread *runningThread;
        bool canceled = false;
};
