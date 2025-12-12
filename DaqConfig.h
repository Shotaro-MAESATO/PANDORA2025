/*-------------------------
deco.mpv analyzer config


-----------------------------*/ 

// DAQ SETUP
//v1730 config
#define N_FADC_BOARD 5
#define RECORD_LENGTH 800
#define N_CH 16

//MADC config
#define N_MADC 6
#define MADC_CH 32

// v1190 config
#define N_V1190 2
#define V1190_CH 128
// v1190 config
//const int trigger_index = 255;

//SIS3820 config
#define SIS3820_CH 32

// MPV Config
#define N_MPV 2

// V7XX Config Board 0 -> ADC, Board 1 -> QDC                                                                          
#define N_V7XX 0
#define V7XX_CH 0