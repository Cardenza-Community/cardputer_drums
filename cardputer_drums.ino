#ifdef CARDENZA_TARGET
#include "cardenza/cardenza_hal.h"
#include "cardenza/cardenza_m5_audio.h"
#endif
#include <SD.h>
#include <sd_defines.h>
#include <sd_diskio.h>

#include <SD.h>
#include <sd_defines.h>
#include <sd_diskio.h>


/**
 * @file cardputer_drums
 * @author John H. Williamson
 * @brief M5Cardputer Drum machine
 *
 *
 * @Hardwares: M5Cardputer
 * @Platform Version: Arduino M5Stack Board Manager v2.0.7
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 */

// TODO:

// beat slicer? (ptr/offset?)
// fix waveform not showing in rec
// sort out kit loading / saving adjustments
// fix pattern switch visual sync (off by 1/2 beat)
// maybe:

// live recording
// bluetooth audio (see: https://github.com/pschatzmann/ESP32-A2DP)
// chords in kits
// kit editor
// basic undo
// polyrhythm patterns (different channel lengths)
// headphone USB-C audio


#include <M5Cardputer.h>
#include "src/kits.h"
#include "src/datatypes.h"
#include "src/config.h"
#include "src/audio.h"
#include "src/ui.h"
#include "src/patternui.h"
#include "src/previewui.h"
#include "src/flash.h"

// Function Declarations
void initCardputer();
void setup(void);
void loop(void);

static struct DrumMachine dm; // global drum machine state

// Initialize the Cardputer
void initCardputer()
{
  auto cfg = M5.config();
#ifdef CARDENZA_TARGET
    Serial.begin(115200);
    const bool cardenzaCodecReady = cardenza_hal_init(32, 16);
    cfg.fallback_board = m5::board_t::board_M5Cardputer;
    cfg.internal_imu = false;
#endif
  M5Cardputer.begin(cfg);
#ifdef CARDENZA_TARGET
    Serial.printf("[Cardenza] ES8156 %s; I2S16/32fs; no gyro/battery/WS2812; heap=%u\n",
                  cardenzaCodecReady ? "ready" : "FAILED", ESP.getFreeHeap());
    // Feed both ES8156 output channels. playRaw(false) mono input is duplicated by M5Unified.
    M5Cardputer.Speaker.end();
    auto cardenzaSpeaker = M5Cardputer.Speaker.config();
    cardenzaSpeaker.stereo = true;
    M5Cardputer.Speaker.config(cardenzaSpeaker);
    if (!cardenzaCodecReady) {
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.setCursor(4, 4);
        M5Cardputer.Display.println("ES8156 INIT FAILED");
        while (true) delay(100);
    }
#endif

  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Speaker.setVolume(255);

#ifdef CARDENZA_TARGET
    cardenza_m5_require(M5Cardputer.Speaker.begin(),"Speaker init FAILED");
#else
    M5Cardputer.Speaker.begin();
#endif
  initLittleFS();  
  initSD();
}

void splash()
{
  auto splashSprite = M5Cardputer.Display; 
  splashSprite.clearDisplay(TFT_WHITE); 

  splashSprite.setFont(&fonts::Orbitron_Light_24);
  splashSprite.setTextDatum(top_center);
  splashSprite.setTextColor(TFT_BLACK);
  splashSprite.drawString("BONNETHEAD", M5Cardputer.Display.width() / 2 , M5Cardputer.Display.height() / 2 - 50);
  
  splashSprite.setTextColor(TFT_BLACK);
  splashSprite.drawString(VERSION, M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2 - 20 );
  splashSprite.setFont(&fonts::Font0);
  // draw a rectangle for the lower text
  splashSprite.fillRect(0, M5Cardputer.Display.height() / 2 + 45, M5Cardputer.Display.width(), 55, RGB565(0,0,0));
  // draw text in small font below  
  splashSprite.setTextColor(TFT_WHITE);
  splashSprite.drawString("W I L L I A M S O N  I N D U S T R I E S", M5Cardputer.Display.width() / 2 , M5Cardputer.Display.height() / 2 + 52 );
}


void setup(void)
{    
  initCardputer();
  Serial.println("\n\n\nBonnetHead "+VERSION+"\n\n\n");
  initState(dm);
  splash();      
  resetState(dm);
  String fname = "/startup.drm";
  bool success = loadDrumMachine(dm, fname);  
  if(!success)
  {
    dm.kit = -1;
    resetState(dm);
  }

  if(dm.kit==-1)
    setKit(dm, 1); // set the default kit if one didn't get loaded.
  // clear the splash and redraw
  dm.splashFlag = false;
  resetMix(dm);
  updatePattern(dm);
}

void loop(void)
{
  M5Cardputer.update();
  updateUI(dm);
  delay(1);
}