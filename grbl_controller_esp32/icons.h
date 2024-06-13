#ifndef _icons_h
#define _icons_h

#include "config.h"

#if defined( TFT_SIZE) and (TFT_SIZE == 3)

const uint8_t logoIcon[] = {
#include "icons/bmp74/logo.h"  
};

const uint8_t setupIcon[] = {
#include "icons/bmp74/setup1_button.h"  
};
const uint8_t printIcon[] = {
#include "icons/bmp74/mill1_button.h"  
};
const uint8_t homeIcon[] = {
#include "icons/bmp74/home1_button.h"  
};
const uint8_t unlockIcon[] = {
#include "icons/bmp74/unlock1_button.h"  
};
const uint8_t resetIcon[] = {
#include "icons/bmp74/reset1_button.h"  
};
const uint8_t sdIcon[] = {
#include "icons/bmp74/sdcard1_button.h"  
};
const uint8_t usbIcon[] = {
#include "icons/bmp74/usb1_button.h"  
};
const uint8_t telnetIcon[] = {
#include "icons/bmp74/telnet1_button.h"  
};
const uint8_t pauseIcon[] = {
#include "icons/bmp74/pause1_button.h"  
};
const uint8_t cancelIcon[] = {
#include "icons/bmp74/cancel1_button.h"  
};
const uint8_t infoIcon[] = {
#include "icons/bmp74/info1_button.h"  
};
const uint8_t cmdIcon[] = {
#include "icons/bmp74/cmd1_button.h"  
};
const uint8_t moveIcon[] = {
#include "icons/bmp74/move1_button.h"  
};
const uint8_t resumeIcon[] = {
#include "icons/bmp74/resume1_button.h"  
};
const uint8_t stopIcon[] = {
#include "icons/bmp74/stop_PC1_button.h"  
};
const uint8_t xpIcon[] = {
#include "icons/bmp74/arrow_Xright1_button.h"  
};
const uint8_t xmIcon[] = {
#include "icons/bmp74/arrow_Xleft1_button.h "  
};
const uint8_t ypIcon[] = {
#include "icons/bmp74/arrow_Yup1_button.h"  
};
const uint8_t ymIcon[] = {
#include "icons/bmp74/arrow_Ydown1_button.h"  
};
const uint8_t zpIcon[] = {
#include "icons/bmp74/arrow_Zup1_button.h"  
};
const uint8_t zmIcon[] = {
#include "icons/bmp74/arrow_Zdown1_button.h"  
};
const uint8_t dAutoIcon[] = {
#include "icons/bmp74/auto1_button.h"  
};
const uint8_t d0_01Icon[] = {
#include "icons/bmp74/d0_01_1_button.h"  
};
const uint8_t d0_1Icon[] = {
#include "icons/bmp74/d0_1_1_button.h"  
};
const uint8_t d1Icon[] = {
#include "icons/bmp74/d1_1_button.h"  
};
const uint8_t d10Icon[] = {
#include "icons/bmp74/d10_1_button.h"  
};
const uint8_t setWCSIcon[] = {
#include "icons/bmp74/set_WCS1_button.h"  
};
const uint8_t setXIcon[] = {
#include "icons/bmp74/setX1_button.h"  
};
const uint8_t setYIcon[] = {
#include "icons/bmp74/setY1_button.h"  
};
const uint8_t setZIcon[] = {
#include "icons/bmp74/setZ1(v_2)_button.h"  
};
const uint8_t setXYZIcon[] = {
#include "icons/bmp74/setXYZ1_button.h"  
};
const uint8_t toolIcon[] = {
#include "icons/bmp74/tool1_button.h"  
};
const uint8_t backIcon[] = {
#include "icons/bmp74/back1_button.h"  
};
//const uint8_t leftIcon[] = {
//#include "icons/bmp74/.h "  
//};
//const uint8_t rightIcon[] = {
//#include "icons/bmp74/.h "  
//};
const uint8_t upIcon[] = {
#include "icons/bmp74/up1_button.h"  
};
const uint8_t morePauseIcon[] = {
#include "icons/bmp74/more1_button.h"  
};
const uint8_t pgPrevIcon[] = {
#include "icons/bmp74/previous1_button.h"  
};
const uint8_t pgNextIcon[] = {
#include "icons/bmp74/next1_button.h"  
};
const uint8_t sdShowIcon[] = {
#include "icons/bmp74/see_gcode1_button.h"  
};


//********************************************* 4" TFT
#elif defined(TFT_SIZE) and (TFT_SIZE == 4)

const uint8_t logoIcon[] = {   // currently use the logo defined for 3.2" TFT
#include "icons/bmp74/logo.h"  
};

const uint8_t setupIcon[] = {
#include "icons/bmp74/setup1_button.h"  
};
const uint8_t printIcon[] = {
#include "icons/bmp74/mill1_button.h"  
};
const uint8_t homeIcon[] = {
#include "icons/bmp74/home1_button.h"  
};
const uint8_t unlockIcon[] = {
#include "icons/bmp74/unlock1_button.h"  
};
const uint8_t resetIcon[] = {
#include "icons/bmp74/reset1_button.h"  
};
const uint8_t sdIcon[] = {
#include "icons/bmp74/sdcard1_button.h"  
};
const uint8_t usbIcon[] = {
#include "icons/bmp74/usb1_button.h"  
};
const uint8_t telnetIcon[] = {
#include "icons/bmp74/telnet1_button.h"  
};
const uint8_t pauseIcon[] = {
#include "icons/bmp74/pause1_button.h"  
};
const uint8_t cancelIcon[] = {
#include "icons/bmp74/cancel1_button.h"  
};
const uint8_t infoIcon[] = {
#include "icons/bmp74/info1_button.h"  
};
const uint8_t cmdIcon[] = {
#include "icons/bmp74/cmd1_button.h"  
};
const uint8_t moveIcon[] = {
#include "icons/bmp74/move1_button.h"  
};
const uint8_t resumeIcon[] = {
#include "icons/bmp74/resume1_button.h"  
};
const uint8_t stopIcon[] = {
#include "icons/bmp74/stop_PC1_button.h"  
};
const uint8_t xpIcon[] = {
#include "icons/bmp74/arrow_Xright1_button.h"  
};
const uint8_t xmIcon[] = {
#include "icons/bmp74/arrow_Xleft1_button.h"  
};
const uint8_t ypIcon[] = {
#include "icons/bmp74/arrow_Yup1_button.h"  
};
const uint8_t ymIcon[] = {
#include "icons/bmp74/arrow_Ydown1_button.h"  
};
const uint8_t zpIcon[] = {
#include "icons/bmp74/arrow_Zup1_button.h"  
};
const uint8_t zmIcon[] = {
#include "icons/bmp74/arrow_Zdown1_button.h"  
};
const uint8_t dAutoIcon[] = {
#include "icons/bmp74/auto1_button.h"  
};
const uint8_t d0_01Icon[] = {
#include "icons/bmp74/d0_01_1_button.h"  
};
const uint8_t d0_1Icon[] = {
#include "icons/bmp74/d0_1_1_button.h"  
};
const uint8_t d1Icon[] = {
#include "icons/bmp74/d1_1_button.h"  
};
const uint8_t d10Icon[] = {
#include "icons/bmp74/d10_1_button.h"  
};
const uint8_t setWCSIcon[] = {
#include "icons/bmp74/set_WCS1_button.h"  
};
const uint8_t setXIcon[] = {
#include "icons/bmp74/setX1_button.h"  
};
const uint8_t setYIcon[] = {
#include "icons/bmp74/setY1_button.h"  
};
const uint8_t setZIcon[] = {
#include "icons/bmp74/setZ1_button.h"  
};
const uint8_t setXYZIcon[] = {
#include "icons/bmp74/setXYZ1_button.h"  
};
const uint8_t toolIcon[] = {
#include "icons/bmp74/tool1_button.h"  
};
const uint8_t backIcon[] = {
#include "icons/bmp74/back1_button.h"  
};
//const uint8_t leftIcon[] = {
//#include "icons/bmp74/.h "  
//};
//const uint8_t rightIcon[] = {
//#include "icons/bmp74/.h "  
//};
const uint8_t upIcon[] = {
#include "icons/bmp74/up1_button.h"  
};
const uint8_t morePauseIcon[] = {
#include "icons/bmp74/more1_button.h"  
};
const uint8_t pgPrevIcon[] = {
#include "icons/bmp74/previous1_button.h"  
};
const uint8_t pgNextIcon[] = {
#include "icons/bmp74/next1_button.h"  
};
const uint8_t sdShowIcon[] = {
#include "icons/bmp74/see_gcode1_button.h"  
};


#else
#error TFT_SIZE must be 3 or 4 (see config.h)
#endif




#endif
