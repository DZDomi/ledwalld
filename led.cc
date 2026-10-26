#include "led.h"

Led* Led::instance = nullptr;
std::mutex m;
std::condition_variable cond_var;

typedef int64_t tmillis_t;
static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct ImageParams {
    ImageParams() : anim_duration_ms(distant_future), wait_ms(1500),
                    anim_delay_ms(-1), loops(-1) {}
    tmillis_t anim_duration_ms;  // If this is an animation, duration to show.
    tmillis_t wait_ms;           // Regular image: duration to show.
    tmillis_t anim_delay_ms;     // Animation delay override.
    int loops;
};

struct FileInfo {
    ImageParams params;      // Each file might have specific timing settings
    bool is_multi_frame;
    StreamIO *content_stream;
};

Led* Led::getInstance(){
	if(instance != nullptr){
		return instance;
	}
	instance = new Led();
	setUpInterruptHandler();
	return instance;
}

Led::Led() : color(255, 255, 0), runningThread(nullptr) {
    
    log("led", "Trying to initialize led wall...");
    
    Magick::InitializeMagick(NULL);
    
    RGBMatrix::Options options;
    options.hardware_mapping = "adafruit-hat-pwm";
    options.chain_length = 4;
    options.pixel_mapper_config = "U-mapper";
    options.show_refresh_rate = false;
    
    rgb_matrix::RuntimeOptions runtime;
    runtime.drop_privileges = 1;
    
    this->matrix = rgb_matrix::CreateMatrixFromOptions(options, runtime);
    
    this->font = Font();
    this->font.LoadFont("fonts/8x13.bdf");
    
    this->matrix->SetBrightness(100);
    
    //Initialize random number generator
    srand(time(NULL));
    
    log("led", "Initialization of led wall completed");
    
}

void Led::printText(string text){
    log("led", "Preparing a new thread for showing text");
    this->prepareThread(&Led::showText, text);
}

void Led::printPicture(string data){
    log("led", "Preparing a new thread for showing a picture/gif");
    this->prepareThread(&Led::showPicture, data);
}

void Led::prepareThread(void (*func)(Led *, string), string buffer) {
    this->cancelAction();
    log("led", "Creating new thread and detaching it");
    this->runningThread = new thread(func, this, buffer);
    (*this->runningThread).detach();
}

void Led::cancelAction(){
    //Tell currently running thread to stop execution, if one was started
    if(this->runningThread){
        log("led", "Canceling current running thread...");
        this->canceled = true;
        std::unique_lock<std::mutex> lck(m);
        cond_var.notify_one();
        //Tell the current running thread to check the canceled variable
        cond_var.wait(lck, [&] { return !this->canceled; });
        this->matrix->Clear();
        free(this->runningThread);
        this->runningThread = nullptr;
        log("led", "Successfully canceled running thread");
    }
}

void Led::showText(Led *led, string text){
    std::unique_lock<std::mutex> lck(m);
    int continuum = rand() % 1000 + 1, red, green, blue;
    int pos = led->matrix->width();
    log("led", "Starting to print text: " + text);
    while(!cond_var.wait_for(lck, std::chrono::microseconds(30000), [&]{ return led->canceled; })){
        
        led->matrix->Clear();
        
        led->calculateColor(&continuum, &red, &green, &blue);
        led->color = Color(red, green, blue);
        int len = rgb_matrix::DrawText(led->matrix, led->font, pos, (led->font.baseline() + led->matrix->height()) / 2, led->color, NULL, text.c_str());
        
        pos -= 1;
        if(pos + len < 0){
            pos = led->matrix->width();
            //break;
        }
    }
    //Tell the main thread that we finished execution
    instance->canceled = false;
    instance->runningThread = nullptr;
    cond_var.notify_one();
}

void Led::showPicture(Led *led, string data){
    std::unique_lock<std::mutex> lck(m);
    log("led", "Starting to show prepare and show image/gif");
    while(!cond_var.wait_for(lck, std::chrono::microseconds(30000), [&]{ return led->canceled; })){
        
        led->matrix->Clear();
        
        FrameCanvas *offScreenCanvas = instance->matrix->CreateFrameCanvas();
        std::vector<Magick::Image> imageSequence;
        //Try to read the file into the buffer
        try {
            log("led", "Trying to read image from data...");
            readImageFromBuffer(data, &imageSequence);
        } catch(exception e){
            //Invalid image/gif
            log("led", "Unable to convert data to an image/gif!");
            break;
        }
        FileInfo *fileInfo = new FileInfo();
        fileInfo->params = ImageParams();
        fileInfo->is_multi_frame = imageSequence.size() > 1;
        fileInfo->content_stream = new MemStreamIO();
        
        log("led", "Writing image file into stream buffer...");
        StreamWriter out(fileInfo->content_stream);
        for (size_t i = 0; i < imageSequence.size(); i++) {
            const Magick::Image &img = imageSequence[i];
            int64_t delay_time_us;
            if (fileInfo->is_multi_frame) {
                delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
            } else {
                delay_time_us = fileInfo->params.wait_ms * 1000;  // single image.
            }
            
            if (delay_time_us <= 0)  {
                delay_time_us = 100 * 1000;  // 1/10sec
            }
            storeInStream(img, delay_time_us, offScreenCanvas, &out);
        }
        log("led", "Stream buffer successfully created");
        log("led", "Starting to show image/gif...");
        displayAnimation(fileInfo, offScreenCanvas);
    }
    //Tell the main thread that we finished execution
    instance->canceled = false;
    instance->runningThread = nullptr;
    cond_var.notify_one();
}

void Led::displayAnimation(const FileInfo *fileInfo, FrameCanvas *offscreen_canvas) {
  const tmillis_t duration_ms = (fileInfo->is_multi_frame ? fileInfo->params.anim_duration_ms : fileInfo->params.wait_ms);
  rgb_matrix::StreamReader reader(fileInfo->content_stream);
  int loops = fileInfo->params.loops;
  const tmillis_t end_time_ms = getTimeInMillis() + duration_ms;
  const tmillis_t override_anim_delay = fileInfo->params.anim_delay_ms;
  for (int k = 0; (loops < 0 || k < loops) && !instance->canceled && getTimeInMillis() < end_time_ms; ++k) {
    uint32_t delay_us = 0;
    while (!instance->canceled && getTimeInMillis() <= end_time_ms && reader.GetNext(offscreen_canvas, &delay_us)) {
      const tmillis_t anim_delay_ms = override_anim_delay >= 0 ? override_anim_delay : delay_us / 1000;
      const tmillis_t start_wait_ms = getTimeInMillis();
      offscreen_canvas = instance->matrix->SwapOnVSync(offscreen_canvas);
      const tmillis_t time_already_spent = getTimeInMillis() - start_wait_ms;
      sleepMillis(anim_delay_ms - time_already_spent);
    }
    reader.Rewind();
  }
}

void Led::storeInStream(const Magick::Image &img, int delay_time_us, FrameCanvas *scratch, StreamWriter *output) {
  scratch->Clear();
  const int x_offset = (scratch->width() - img.columns()) / 2;
  const int y_offset = (scratch->height() - img.rows()) / 2;
  for (size_t y = 0; y < img.rows(); ++y) {
    for (size_t x = 0; x < img.columns(); ++x) {
      const Magick::Color &c = img.pixelColor(x, y);
      if (c.alphaQuantum() < 256) {
        scratch->SetPixel(x + x_offset, y + y_offset,
                          ScaleQuantumToChar(c.redQuantum()),
                          ScaleQuantumToChar(c.greenQuantum()),
                          ScaleQuantumToChar(c.blueQuantum()));
      }
    }
  }
  output->Stream(*scratch, delay_time_us);
}

void Led::readImageFromBuffer(string data, std::vector<Magick::Image> *result){
    std::vector<Magick::Image> frames;
    Magick::Blob blob = Magick::Blob(static_cast<const void *>(data.c_str()), data.size());
    readImages(&frames, blob);
    if (frames.size() > 1) {
        Magick::coalesceImages(result, frames.begin(), frames.end());
    } else {
        result->push_back(frames[0]);   // just a single still image.
    }
    
    for (size_t i = 0; i < result->size(); ++i) {
        (*result)[i].scale(Magick::Geometry(instance->matrix->width(), instance->matrix->height()));
    }
}

tmillis_t Led::getTimeInMillis() {
  struct timeval tp;
  gettimeofday(&tp, NULL);
  return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

void Led::sleepMillis(tmillis_t milli_seconds) {
  if (milli_seconds <= 0) return;
  struct timespec ts;
  ts.tv_sec = milli_seconds / 1000;
  ts.tv_nsec = (milli_seconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void Led::calculateColor(int *continuum, int *red, int *green, int *blue){

    *continuum = ((*continuum) + 1) % (3 * 255);
    *red = *green = *blue = 0;

    if (*continuum <= 255){
        int c = *continuum;
        *blue = 255 - c;
        *red = c;
    } else if (*continuum > 255 and *continuum <= 511){
        int c = *continuum - 256;
        *red = 255 - c;
        *green = c;
    } else {
        int c = *continuum - 512;
        *green = 255 - c;
        *blue = c;
    }
}


Led::~Led() {
    matrix->Clear();
    delete matrix;
}

void Led::setUpInterruptHandler() {
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = instance->interrupt;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
}

void Led::interrupt(int) {
    instance->matrix->Clear();
	delete instance->matrix;
}
