#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>
#include <signal.h>
#include <map>
#include <algorithm>
#include <regex>
#include <numeric>
#include <thread>
#include <future>
#include <iomanip>
#include <time.h>
#include <sys/time.h>
#include <TFile.h>
#include <TTree.h>
#include <TH2D.h>

#include "segidlist.h"
#include "MyclassDict.cpp"
#include "DigitalFilter.cpp"

using namespace std;

//#define DEBUG

#ifdef DEBUG
#define DB(x) x
#else
#define DB(x)
#endif

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
const int trigger_index = 255;


//SIS3820 config
#define SIS3820_CH 32

// MPV Config
#define N_MPV 2


enum RIDF_CID{
  RIDF_EF_BLOCK,
  RIDF_EA_BLOCK,
  RIDF_EAEF_BLOCK,
  RIDF_EVENT,
  RIDF_SEGMENT,
  RIDF_COMMENT,
  RIDF_EVENT_TS,
  RIDF_BLOCK_NUMBER=8,
  RIDF_END_BLOCK,
  RIDF_SCALER=11,
  RIDF_NCSCALER=11,
  RIDF_CSCALER=12,
  RIDF_NCSCALER32,
  RIDF_TIMESTAMP=16,
  RIDF_STATUS=21,

};

typedef struct{
  char Revision;
  char Device;
  char FP;
  char Detector;
  char Module;
} SegID;


class CalibPar{
public:
  CalibPar(const std::string &_file, bool _optimize = false)
  {
    std::ifstream ifs(_file.c_str());
    if(!ifs.good())
    return;
    else{
      int board, ch;
      double par0, par1;
      int board_max = 5;
      int ch_max = 15;
      //Input file format : [boardID] [Channel] [par0:intection] [par1:slope]
      while(ifs >> board >> ch >> par1 >> par0){
        //std::cout << board << " " << ch << " " << par0 << " " << par1 <<  std::endl;
        fParameters.push_back(std::make_pair(Address(board, ch), std::make_pair(par0, par1)));
        if(board > board_max)
    board_max = board;
  if(ch > ch_max)
    ch_max = ch;
      }
      
// std::cout << "Bmax, chmax " << board_max << " " << ch_max <<std::endl;
      
      //Make paramater array :  To improve access speeed in GetPar(). Sequential O(N) -> Random O(1)
      if(_optimize){
  fParameterArray = std::vector<std::vector<par_type>>(board_max + 1);
  for(auto &board : fParameterArray){
    board.resize(ch_max + 1);
    for(auto & par: board){
      par = std::make_pair(0,0);
    }
  }
  
  //  std::cout << " a " << fParameterArray.size()  << " " <<  fParameterArray.at(0).size()  <<std::endl;
  for(int iBoard = 0; iBoard < board_max + 1; ++iBoard){
    for(int iCh = 0; iCh < ch_max + 1; ++iCh){
      try{
        auto par = GetPar(iBoard, iCh);
        // std::cout << iBoard << " " << iCh << " " << par.first << " " << par.second << std::endl;
        fParameterArray.at(iBoard).at(iCh) = par;
      }
      catch(...){
        continue;
      }
    }
  }
  fIsOptimized = true;
      } 
    }
  };
  
  using par_type = std::pair<double,double>;
  par_type GetPar(int _board_id, int _ch) const
  {
    if(fIsOptimized){
      try{
        // std::cout <<  _board_id << " / " << fParameterArray.size() << std::endl;
        // std::cout <<  _ch << " / " << fParameterArray.at(_board_id).size()  << std::endl;
        return fParameterArray.at(_board_id).at(_ch);
      }
      catch(...){
        throw std::out_of_range("Invalid access to CalibPar optimized  : board = " + std::to_string(_board_id) + " ch = " +std::to_string(_ch));
      }
      
    }
    else{//if(fIsOptimized = false){
      auto it = std::find_if(fParameters.begin(),
      fParameters.end(),
           [&](const std::pair<Address, par_type> &_par)->bool{
        return _par.first == Address(_board_id, _ch);
           });
      if(it != fParameters.end())
      {
        return it->second;
      }
      throw std::out_of_range("Invalid access to CalibPar : board = " + std::to_string(_board_id) + " ch = " +std::to_string(_ch));
    }
    
    
  }
  
  struct Address{
    Address() : fBoard(-1), fCh(-1){};
    Address(int _fBoard, int _fCh) : fBoard(_fBoard), fCh(_fCh){};
    bool operator==(Address _rhs) const
    {
      return fBoard == _rhs.fBoard && fCh == _rhs.fCh;
    };
    
    int fBoard, fCh;
  };
  
  
  bool IsOptimized()const{return fIsOptimized;};
  
private:
  
  std::vector<std::pair<Address, par_type>> fParameters;
  bool fIsOptimized;
  std::vector<std::vector<par_type>> fParameterArray; // < -- for optimization
  
};

double GetSlope(const CalibPar& _par, int _iBoard, int _iCh){
  return _par.GetPar(_iBoard, _iCh).first;
}

double GetInterSection(const CalibPar& _par, int _iBoard, int _iCh){
  return _par.GetPar(_iBoard, _iCh).second;
}

//Define inpur par file
std::string GetParameterFileForRunNo(int _run_no){
  return "param/energy/FADC.prm";
}

// std::string GetParameterFileForRunNo(int Board){
//   return Form("param/energy/FADC_%d.prm", Board);
// }

// MADC calibration file
  std::string GetParameterFile2ForRunNo(int _run_no){
    return "param/energy/MADC.prm";
  }

  //Get Run No from RIDF path
int ExtractRunNo(const std::string& _ridf_path){
  std::string obj_str = _ridf_path;
  std::smatch smatch;
    std::regex_search(obj_str, smatch, std::regex("\\d{4}.ridf$"));
    if(smatch.size() == 0){
      throw std::runtime_error("ExtractRunNo(): NO pattern found. - > " + _ridf_path);
    }
    else if (smatch.size() == 1){
      std::string run_no = smatch[0].str().substr(0,4);
      std::cerr << "OK run " << run_no << std::endl;
      return std::stoi(run_no);
    }
    else{
      throw std::runtime_error("ExtractRunNo(): Multiple hit. " + _ridf_path);
    }
}
std::vector<double> trapezoidal(const std::vector<double> &wave, int L, int G) {
  std::vector<double> ret;
  //  int length = wave.size();
  int length = RECORD_LENGTH;
  for (int j=0, n=length-(2*L+G); j<n; j++){
    float tmp = 0;
    for (int k=0; k<L; k++){
      tmp += static_cast<float>(wave[j+k]) * (-1/static_cast<float>(L));
    }
    for (int k=0; k<G; k++){
      tmp += wave[j+k+G] * 0;
    }
    for (int k=0; k<L; k++){
      tmp += static_cast<float>(wave[j+k+G+L]) * (1/static_cast<float>(L));
    }
    ret.push_back(tmp);
  }
  return ret;
}

  
typedef struct{
    char name[100];
    char num[100];
    char start[20];
    char stop[20];
    char date[60];
    char comment[200];
} RunInfo;

int nch[N_FADC_BOARD]={16,16,16,16,16};
uint32_t v1730_header[4];
uint32_t v1730_ch_header;
uint64_t trigger_time_tag[N_FADC_BOARD];
uint64_t trigger_time_tag_pre[N_FADC_BOARD];
int ch_size; // 32-bit word
uint32_t skipped_samples;
vector<vector<uint16_t>> fadc_raw_data[N_FADC_BOARD];
vector<double> data_bsub;
vector<double> data_basub;
vector<vector<double>> data_fil[N_FADC_BOARD];
vector<vector<double>> data_tra[N_FADC_BOARD];
int Clk[RECORD_LENGTH];
uint data_index;
int baseline[N_FADC_BOARD][N_CH];
double baseline_ave[N_FADC_BOARD][N_CH];
int sum[N_FADC_BOARD][N_CH];
int ADC[N_FADC_BOARD][N_CH];
double ADC_cor[N_FADC_BOARD][N_CH];
double Energy[N_FADC_BOARD][N_CH];
int PeakClk[N_FADC_BOARD][N_CH];
int RiseEdge[N_FADC_BOARD][N_CH];
int ADC_sum[N_FADC_BOARD][N_CH];
int adc_sum[N_FADC_BOARD][N_CH];

const int V1190_total_ch = N_V1190 * V1190_CH;
vector<vector<int>> TDC(V1190_total_ch,vector<int>(1,0));
uint32_t TTT_V1190[N_V1190];
uint32_t V1190hit[V1190_total_ch];
Int_t TDC_sub_Trig[V1190_total_ch];
// Trigger time tags for event matching 2021/06/2
uint64_t TTT_V1730_rev[N_FADC_BOARD];

// double TimeV1730[N_FADC_BOARD]={};
// double TimeV1730_pre[N_FADC_BOARD]={};
// Trigger time tags for event matching

// MADC 
double Energy_MADC[N_MADC][MADC_CH];
int MADC[N_MADC][MADC_CH];
uint64_t Extended_TS[N_MADC]={};
uint64_t TTT_MADC[N_MADC]={};
uint32_t TTT_MADC_pre[N_MADC]={};
uint64_t TTT_MADC_rev[N_MADC];
double TimeMADC[N_MADC]={};
double TimeMADC_pre[N_MADC]={};
uint64_t MADC_TS[N_MADC]={};
uint32_t scaler[32]={};

bool evt_stored=false;
bool quit_flag=false;

TFile* file;
TTree* tree;
TH2D* waveform[N_FADC_BOARD][N_CH];
TH2D *energy[N_FADC_BOARD];
TH2D* ADC_ch;                //20240703

double Amax[N_FADC_BOARD][N_CH];
//MPV
uint64_t MPV_TS[N_MPV] ;
uint64_t high16[N_MPV] ;
uint64_t low32[N_MPV] ;
int MPV_10kclk[N_MPV] ;
int MPV_evtn[N_MPV] ;

// Scaler
uint32_t SIS3820_scaler[SIS3820_CH];



void stop_dataread(int sig){
  quit_flag=true;
  evt_stored=false;
}

class TrigTimeTags{
public:
  TrigTimeTags(){
    Clear();
  };
  
  void Clear()
  {
    for(int iCh = 0; iCh < N_MADC; ++iCh){
      TTT_MADC[iCh] = 0;
    }
    for(int iCh = 0; iCh < N_FADC_BOARD; ++iCh){
      TTT_V1730[iCh] = 0;
    }
  }
  
  uint64_t GetTTT_V1730(int i)
  {
    if(i < 0 ||i >= N_FADC_BOARD)
      return 0xffffffffffffffff;
    else
      return TTT_V1730[i];
  }
  
  
  double GetTimeV1730(int i)
  {
    if(i < 0 ||i >= N_FADC_BOARD)
      return -1;
    else
      return (double)TTT_V1730[i]/(double)freqV1730;
  }
  uint64_t GetTTT_MADC(int i)
  {
    if(i < 0 ||i >= N_MADC)
      return 0xffffffffffffffff;
    else
      return TTT_MADC[i];
  }
  
  
  
  double GetTimeMADC(int i)
  {
    if(i < 0 ||i >= N_MADC)
      return -1;
    else
      return (double)TTT_MADC[i]/(double)freqMADC;
  }  
  void Update(uint64_t TTT_MADC_in[N_MADC], uint64_t TTT_V1730_in[N_FADC_BOARD]){
    for(int iCh = 0; iCh < N_MADC; ++iCh){
      uint64_t fraction = TTT_MADC[iCh] % TTT_MADC_MAX;
      unsigned int  n_loop  = floor(TTT_MADC[iCh] / TTT_MADC_MAX);
      if(TTT_MADC_in[iCh] >= fraction) //normal order
        TTT_MADC[iCh] = n_loop * TTT_MADC_MAX + TTT_MADC_in[iCh];
      else{ // clock loop
  //std::cout << "MADC increment " << std::endl;
  //std::cout << iCh << " " << TTT_MADC_in[iCh] << " -> " << TTT_MADC[iCh] <<std::endl;
  TTT_MADC[iCh] = (n_loop + 1) * TTT_MADC_MAX + TTT_MADC_in[iCh];
  //std::cout << "     ->  " << TTT_MADC[iCh] <<std::endl;
      }
      }
  }
private:
  uint64_t TTT_V1730[N_FADC_BOARD];
  static constexpr uint64_t TTT_V1730_MAX = pow(2, 48);
  static constexpr double freqV1730 = 125.0e+6;//(Hz)
  uint64_t TTT_MADC[N_MADC];
  static constexpr uint64_t TTT_MADC_MAX = pow(2, 30);
  static constexpr double freqMADC = 62.5e+6;//(Hz)
};





int main(int argc, char *argv[]){
  uint32_t buf_header[2]; // [header,address]
  uint32_t buf_size;
  uint32_t evtn;
  uint64_t ts;
  uint32_t buf_segid;
  uint32_t *buff;
  uint32_t date,scrid;
  int cid,blksize;
  int segdatasize,scrdatasize;
  int datasize;
  unsigned int blkn=-1;
  char comment[100000];
  int board;
  char hnam[100];
  RunInfo run_info;
  SegID seg_id;
  uint64_t evt_stop=-1;
  
  // time-mes
  struct timeval sT_all,eT_all,dT_all;
  struct timeval sT_fill,eT_fill,dT_fill;
  struct timeval sT_filter,eT_filter,dT_filter;
  struct timeval tempT;
  int filter_cnt=0;
  
  int FADC_hit[N_FADC_BOARD]={};
  
  // vector<future<int>> threadpool;
  int bp=0;
  // time-mes
  gettimeofday(&sT_all,NULL);
  
  signal(SIGINT, stop_dataread);
  
    //if(argc!=3){
  if(argc<3){
    cerr << argv[0] << " ifile ofile [use_Amax] [evt_stop]" << endl;
    return 0;
  }
  bool use_Amax = false;
  if(argc>=4){
    string arg4 = argv[3];
    if(arg4 == "true" || arg4 == "1" || arg4 == "T" || arg4 == "t"){
      use_Amax = true;
      cout << "Amax fillter on" << endl;
    }
  }

  if(argc>=5){
    evt_stop=atoi(argv[4]);
  }
  
  CalibPar calibpar(GetParameterFileForRunNo(ExtractRunNo(argv[1])), true);
  std::cout << GetParameterFileForRunNo(ExtractRunNo(argv[1])) << std::endl;
  CalibPar calibpar2(GetParameterFile2ForRunNo(ExtractRunNo(argv[1])), true);
  std::cout << GetParameterFile2ForRunNo(ExtractRunNo(argv[1])) << std::endl;
  ifstream fin(argv[1], ios::in|ios::binary);
  
  if(!fin.is_open()){
    cerr << "\033[31mError: faile to open" << argv[1] << "\033[m " << endl;
    return 0;
  }
  ofstream fsca;
  string fsca_name;
  
  buff = new uint32_t[0x800000];
  
  for(int i=0;i<N_FADC_BOARD;++i){
    fadc_raw_data[i].resize(N_CH);
    data_fil[i].resize(N_CH);
    data_tra[i].resize(N_CH);
    for(int j=0;j<N_CH;++j) fadc_raw_data[i][j].reserve(RECORD_LENGTH);
  }
  
  file = new TFile(argv[2],"RECREATE");
  tree = new TTree("tree","tree");
  tree->SetMaxTreeSize(1000000000000LL);
  

  tree->SetAutoSave(500000000); // 500 MB save
  tree->SetAutoFlush(10000); 
  /*
  tree->SetAutoSave(5000);
  tree->SetAutoFlush(5000);
  */
  char bnam[100];
  //v1190
  tree->Branch("TDC","std::vector<std::vector<int>>",&TDC);
  tree->Branch("TDC_sub",TDC_sub_Trig,Form("TDC_sub_Trig[%d]/I",V1190_total_ch));
  sprintf(bnam,"TTT_V1190/i");
  tree->Branch("TTT_V1190",&TTT_V1190,bnam);
  tree->Branch("V1190hit",V1190hit,Form("V1190hit[%d]/I",V1190_total_ch));

  // v1730
  sprintf(bnam,"TTT[%d]/l",N_FADC_BOARD);
  tree->Branch("TTT",trigger_time_tag,bnam);  
  // sprintf(bnam,"TTT_pre[%d]/l",N_FADC_BOARD);
  // tree->Branch("TTT_pre",trigger_time_tag_pre,bnam);  
  sprintf(bnam,"baseline[%d][%d]/I",N_FADC_BOARD,N_CH);
  tree->Branch("baseline",baseline,bnam);
  sprintf(bnam,"baseline_ave[%d][%d]/D",N_FADC_BOARD,N_CH);
  tree->Branch("baseline_ave",baseline_ave,bnam);
  sprintf(bnam,"ADC[%d][%d]/I",N_FADC_BOARD,N_CH);
  tree->Branch("ADC",ADC,bnam);
  //  sprintf(bnam,"sum[%d][%d]/I",N_FADC_BOARD,N_CH);
  //  tree->Branch("sum",sum,bnam);
  sprintf(bnam,"ADC_cor[%d][%d]/D",N_FADC_BOARD,N_CH);
  tree->Branch("ADC_cor",ADC_cor,bnam);
  sprintf(bnam,"Energy[%d][%d]/D",N_FADC_BOARD,N_CH);
  tree->Branch("Energy",Energy,bnam);
  sprintf(bnam,"PeakClk[%d][%d]/I",N_FADC_BOARD,N_CH);
  tree->Branch("PeakClk",PeakClk,bnam);
  //  sprintf(bnam,"RiseEdge[%d][%d]/I",N_FADC_BOARD,N_CH);
  //  tree->Branch("RiseEdge",RiseEdge,bnam);
  sprintf(bnam,"Amax[%d][%d]/D",N_FADC_BOARD,N_CH);
  tree->Branch("Amax",Amax,bnam);
  //  sprintf(bnam,"TTT_V1730_r[%d]/l",N_FADC_BOARD);
  //  tree->Branch("TTT_V1730_r",TTT_V1730_rev,bnam);  
  //  sprintf(bnam,"TimeV1730[%d]/d",N_FADC_BOARD);
  //  tree->Branch("TimeV1730",TimeV1730,bnam);
  
  
  // MADC
  sprintf(bnam,"MADC[%d][%d]/I",N_MADC,32);
  tree->Branch("MADC",MADC,bnam);
  sprintf(bnam,"Energy_MADC[%d][%d]/D",N_MADC,32);
  tree->Branch("Energy_MADC",Energy_MADC,bnam);
  sprintf(bnam,"TTT_MADC[%d]/l",N_MADC);
  tree->Branch("TTT_MADC",TTT_MADC,bnam);


  // Scaler
  sprintf(bnam,"SIS3820_scaler[%d]/i",SIS3820_CH);
  tree->Branch("SIS3820_scaler",SIS3820_scaler,bnam);
  
  //MPV
  tree->Branch("MPV_TS",MPV_TS,Form("MPV_TS[%d]/l",N_MPV));

  for(int i=0;i<N_FADC_BOARD;++i) tree->Branch(Form("rawdata_%d",i),"std::vector<std::vector<uint16_t>>",&fadc_raw_data[i]);
  //for(int i=0;i<N_FADC_BOARD;++i) tree->Branch(Form("data_fil_%d",i),"std::vector<std::vector<double>>",&data_fil[i]);
  
  /** SET HISTOGRAM **/
  for(int i=0;i<N_FADC_BOARD;i++){
    for(int j=0;j<N_CH;j++){
      sprintf(hnam,"wf%d_%02d",i,j);
      waveform[i][j]=new TH2D(hnam,hnam,RECORD_LENGTH/4,0,RECORD_LENGTH,1024,0,16384);
    }
  }
  int fadc_ch_all;
  fadc_ch_all = N_FADC_BOARD * N_CH;
  ADC_ch = new TH2D("fadc","FADC vs ch",fadc_ch_all,0,fadc_ch_all,1024,0,16384); //20240703s
  
  //MADC
  TH2D* MADCvsCH[N_MADC];
  for (int i = 0; i < N_MADC; i++){
    MADCvsCH[i] = new TH2D(Form("madc%d",i),Form("madc%d vs ch",i),32,0,32,8192,0,8192);
  }

  // SAKRA pid histogram
  TH2D* pid_1st[16];
  for(int i=0; i<16; i++){
    pid_1st[i] = new TH2D(Form("hpid_1st_ch%d", i), Form("Amax vs Energy ch%d", i),
        512, 0, 20, 512, 0, 2.0);
    pid_1st[i]->SetDrawOption("colz");
    //serv->Register("pid_1st", pid_1st[i]);
  }
  
  
  TrigTimeTags ttts;
  while(!fin.eof()){
    fin.read((char*)buf_header, sizeof(buf_header));
    cid= (buf_header[0] & 0x0FC00000) >> 22;
    blksize= buf_header[0] & 0x003FFFFF; //2 byte
    DB(cout << "cid=" << cid << endl);
    switch(cid){
    case RIDF_EF_BLOCK:
    case RIDF_EA_BLOCK:
    case RIDF_EAEF_BLOCK:
    ++blkn;
    DB(printf("EF Block Header / blkn=%d\n",blkn));
    break;
    case RIDF_END_BLOCK:
    DB(printf("EF Block Ender / blkn=%d\n",blkn));
    fin.read((char*)&buf_size, sizeof(buf_size));
    DB(printf("Size of this block=%d\n",buf_size));
    break;
    case RIDF_BLOCK_NUMBER:
      DB(printf("EF Block Number / blkn=%d\n",blkn));
      fin.read((char*)&buf_size, sizeof(buf_size));
      DB(printf("The Number of this block=%d\n",buf_size));
      break;
    case RIDF_EVENT:
    case RIDF_EVENT_TS:
      if(evt_stored){
  // for(auto &th : threadpool) th.get();
  // threadpool.clear();
  /*
    if(evtn%100!=0){
    for(int i=0;i<N_FADC_BOARD;++i){
    for(int j=0;j<N_CH;++j) fadc_raw_data[i][j].clear();
    }
    }
  */
  ttts.Update(TTT_MADC_rev, TTT_V1730_rev);
  
  for(int board = 0; board < N_FADC_BOARD; ++board){
    TTT_V1730_rev[board] = ttts.GetTTT_V1730(board);
    //    TimeV1730[board] = ttts.GetTimeV1730(board);
    //    std::cout << ttts.GetTimeV1730(board)<<std::endl;;
  }
  
  // time-mes
  gettimeofday(&sT_fill,NULL);
  tree->Fill();
  gettimeofday(&eT_fill,NULL);
  timersub(&eT_fill,&sT_fill,&tempT);
  timeradd(&dT_fill,&tempT,&dT_fill);
  
  for(Int_t i=0;i< N_MADC;++i) TTT_MADC_pre[i]=TTT_MADC[i];
  for(Int_t i=0;i<N_FADC_BOARD;++i)
  trigger_time_tag_pre[i]=trigger_time_tag[i];
  
  evt_stored=false;
}
/***************** Data initialization *****************/
for(int i=0;i<V1190_total_ch;++i) TDC[i].clear();
// for(int i=0;i<V1190_total_ch;++i) TDC[i].assign(1, -1e6); ;
for (int i = 0; i < N_V1190; i++)TTT_V1190[i]=0;
for(int i=0;i<V1190_total_ch;++i) {
  V1190hit[i]=0;
  TDC_sub_Trig[i] = -1e6;
}

// MADC
for(int i=0;i<N_MADC;++i){
  TimeMADC_pre[i]=TimeMADC[i];
  TimeMADC[i]=0;
  TTT_MADC[i]=0;
  TTT_MADC_rev[i]=0;
  for(int j=0;j<MADC_CH;++j){
    MADC[i][j]=-1;
    Energy_MADC[i][j]=-1;
  }
}

//SIS3820
for(int i=0;i<SIS3820_CH;++i) SIS3820_scaler[i]=0;

// MPV
for (int i = 0; i < N_MPV; i++){
  MPV_TS[N_MPV] = 0;
  high16[N_MPV] = 0 ;
  low32[N_MPV] = 0 ;
  MPV_10kclk[N_MPV] = 0;
  MPV_evtn[N_MPV] = 0;
}


for(int board=0;board<N_FADC_BOARD;++board){
  for(int ch=0;ch<nch[board];++ch){
    ADC[board][ch]=-1e6;
    ADC_cor[board][ch]=-1e6;
    baseline_ave[board][ch]=0;
    Energy[board][ch]=-1;
    PeakClk[board][ch]=-1;
    RiseEdge[board][ch]=-1;
    fadc_raw_data[board][ch].clear();
    data_fil[board][ch].clear();
    data_tra[board][ch].clear();
    baseline[board][ch]=-1;
    sum[board][ch]=0;
    //maesato add
    Amax[board][ch]=-1;
    adc_sum[board][ch]=0;
    ADC_sum[board][ch]=0;
  }
  //  TimeV1730_pre[board]=TimeV1730[board];
  //  TimeV1730[board]=0;
  trigger_time_tag[board]=0;
  FADC_hit[board]=0;
}

for(int board=0;board<N_FADC_BOARD;++board){TTT_V1730_rev[board] = 0;
}

/*******************************************************/

//      if(evtn%10000==0) evt_stored=true;
fin.read((char*)&evtn, sizeof(evtn));
if(evtn==evt_stop){
  pid_t pid=getpid();
  kill(pid,SIGINT);
}
evt_stored=true;

if(cid==RIDF_EVENT_TS){
  fin.read((char*)&ts, sizeof(ts));
  ts &= 0x0000ffffffffffff;
  DB(printf("Event Hearder with Time Stamp / blkn=%d\n",blkn));
  DB(printf("Event Number = %d\n",evtn));
  DB(printf("Time Stamp = %ld\n",ts));  
}
else{
  DB(printf("Event Hearder / blkn=%d\n",blkn));
  DB(printf("Event Number = %d\n",evtn));
}
if(evtn%1000==0){
  printf("\rAnalyzing Event Number: %d",evtn);
  fflush(stdout);
}
break;
case RIDF_SEGMENT:
fin.read((char*)&buf_segid, sizeof(buf_segid));
seg_id.Revision=(buf_segid & 0xFC000000) >> 26;
seg_id.Device=(buf_segid & 0x03F00000) >> 20;
seg_id.FP=(buf_segid & 0x000FC000) >> 14;
seg_id.Detector=(buf_segid & 0x3F00) >> 8;
seg_id.Module=(buf_segid & 0x00FF);
board=seg_id.Detector;
/*
if((blksize-(sizeof(buf_header)+sizeof(buf_segid))/2)==0){
//  printf("\nError: Event_N:%d, board:%d has no Segment fadc_raw_data\n",evtn,board);
break;
}
*/
fin.read((char*)buff, 2*(blksize-(sizeof(buf_header)+sizeof(buf_segid))/2));
datasize=(blksize-(sizeof(buf_header)+sizeof(buf_segid))/2)/2; //32 bit long word unit

//if(evtn%5000!=0) break;
DB(printf("Segment Hearder / blkn=%d\n",blkn));
DB(printf("Segment ID = %d (0x%08x)\n", buf_segid, buf_segid));

switch(seg_id.Module){
  case V1190:
  bp=0;
  uint32_t tmpbuf;
  int evtcnt,geo;
  int error_flag;
  while(bp<datasize){
    int ch;
    int tmp_ch;
    tmpbuf=buff[bp++];
    if((tmpbuf>>27) == 0x8){ /** Global Header **/
      evtcnt=(tmpbuf&0x07FFFFE0)>>5;
      geo=tmpbuf&0x1F;
    }
    else{
      printf("Invalid Global Header (V1190) : blkn=%d evtn=%d\n",blkn,evtn);
    }
    /** TDC loop **/
    for(int i=0;i<4;++i){
      tmpbuf=buff[bp++];
      if((tmpbuf>>27) == 0x1){ /** TDC Header **/
        int wcnt=1;
        while(bp<datasize){
          tmpbuf=buff[bp++];
          ++wcnt;
          if((tmpbuf>>27)==0x3){ /** TDC Trailer **/
            if((tmpbuf&0xFFF)!=wcnt){
              printf("V1190 TDC %d : Inconsistent word count (count:%d read:%d)\n",i,wcnt,(tmpbuf&0xFFF));
            }
            break;
          }
          if((tmpbuf>>27)==0x4){ /** TDC Error **/
            error_flag=tmpbuf&0x7FFF;
            printf("V1190 TDC %d : Error flag 0x%x\n",i,error_flag);
          }
          if((tmpbuf>>27)==0x0){ /** TDC fadc_raw_data **/
            tmp_ch=(tmpbuf&0x03F80000)>>19;
            ch = tmp_ch + board * V1190_CH;
            TDC[ch].push_back(tmpbuf&0x0007FFF);
            V1190hit[ch]+=1;
          }
        }
      }
      else{
        printf("Invalid TDC Header (V1190) : blkn=%d evtn=%d\n",blkn,evtn);
      }
    }
    while(bp<datasize){
      tmpbuf=buff[bp++];
      if((tmpbuf>>27)==0x11){ /** Extended Trigger Time Tag **/
        TTT_V1190[board]=TTT_V1190[board]|((tmpbuf&0x07FFFFFF)<<5);
      }
      if((tmpbuf>>27)==0x10){ /** Global Trailer **/
        TTT_V1190[board]=TTT_V1190[board]|(tmpbuf&0x1F);
      }
    }
    // tdc analysis
    if (trigger_index < 0 || trigger_index >= V1190_total_ch) {
      printf("Invalid trigger_index %d\n", trigger_index);
    } else if (TDC[trigger_index].empty()) {
      printf("Trigger channel %d has no data in evtn=%d\n", trigger_index, evtn);
    } else {
      int Trigger_timeing = TDC[trigger_index].at(0);
      for (int ch = 0; ch < V1190_total_ch; ++ch) {
        if (V1190hit[ch] == 1 && !TDC[ch].empty()) {
          int TDC_tmp = 0;
          TDC_tmp = TDC[ch].at(0);
          TDC_sub_Trig[ch] = TDC_tmp - Trigger_timeing;
        } else {
          // skipp
        }
      }
    }
    
  }
  break;
  
  case MADC32:
  int sig;
  sig=buff[0]>>30;
  if(sig==1){
    int nword;
    nword=(buff[0]&0x0FFF);
    for(int i=1;i<nword;++i){
      sig=buff[i]>>30;
      int subheader;
      subheader=(buff[i]&0x00800000)>>23;
      if(sig!=0){
      // printf("Invalid Data word (MADC) : blkn=%d evtn=%d word=%d\n",blkn,evtn,i);
	if(sig==3){
	  // cout << "This Word is Ender" <<endl;
	  int correct_nword = i;
	  nword = correct_nword;
	  break;
	}
      }
      if(subheader==0){
        int ch=((buff[i]>>16) & 0x1F);
        MADC[board][ch]=(buff[i]&0x1FFF);
        if((buff[i]>>14) & 0x1){
          MADC[board][ch]=-1;
        }

        MADCvsCH[board]->Fill(ch,MADC[board][ch]);
        Energy_MADC[board][ch]=(GetSlope(calibpar2, board, ch)*MADC[board][ch])+GetInterSection(calibpar2, board, ch);
      }
      else if(subheader==1){
        Extended_TS[board] = (uint64_t)((buff[i]&0xffff)<<30);
      }
    }
    sig=buff[nword]>>30;
    if(sig!=3){
      cout << "MADC Board Number: " << board << endl;
    printf("Invalid End of Event mark (MADC) : blkn=%d evtn=%d\n",blkn,evtn);
    cout << "----------------------------------------" <<endl;
    }

    MADC_TS[board]=(buff[nword]&0x3FFFFFFF);
    TTT_MADC[board]= Extended_TS[board] | MADC_TS[board];

  }

  // Extended time Stamp enable to decode when Invalid Header come
  else if(sig==0){
    printf("Invalid Event Header (MADC) : blkn=%d evtn=%d\n",blkn,evtn);
    int k = 0;
    while(sig == 0){
      int ch=((buff[k]>>16) & 0x1F);
      MADC[board][ch]=(buff[k]&0x1FFF);
      //      adc->Fill(ch,MADC[ch]);
      // MADC_hist[ch]->Fill(MADC[ch]);
      //Energy_MADC[board][ch]=(GetSlope(calibpar2, 0, ch)*MADC[ch])+GetInterSection(calibpar2, 0, ch);
      if((buff[k]>>14) & 0x1){ //out of renge
        MADC[board][ch]=-1;
      }
      k++;
      sig=buff[k]>>30;
    }
    if(sig==3){
      cout << "NO Header but problem sloved " <<endl;
      cout << "loop : "<< k <<endl;
      cout << "----------------------------------------" <<endl;
    }
    else{
      printf("Invalid End of Event mark (MADC) : blkn=%d evtn=%d\n",blkn,evtn);
      cout << "----------------------------------------" <<endl;
    }
    TTT_MADC[board]=(buff[k]&0x3FFFFFFF);
    cout << "TTT_MADC : "<< TTT_MADC[board] <<endl;
  }
  else {
    printf("Invalid sig pattern (MADC): sig=%d, blkn=%d evtn=%d\n", sig, blkn, evtn);
  }
  break;
  
  case V1730ZLE:
  DB(printf("header0=%x, eventsize=%d\n",buff[0],buff[0] & 0x0FFFFFFF));
  DB(printf("board=%d, event counter=%d\n",board,buff[2] & 0x00FFFFFF));
  
  trigger_time_tag[board]=((uint64_t)(buff[1]&0x00FFFF00))<<24 | buff[3];
  //TTT_V1730_rev[board] = trigger_time_tag[board];// copy
  bp=4;
  
  if(datasize!=4+16*2) FADC_hit[board]=1;
  
  for(int ch=0;ch<nch[board];++ch){
    baseline[board][ch]=(buff[bp] & 0x3FFF0000) >> 16;
    ch_size=buff[bp++] & 0x0000FFFF;
    DB(printf("ch%d size=%d\n",ch,ch_size));
    
    if(ch_size==2){
      ++bp;
      continue;
    }
    
    for(int i=0;i<(ch_size-1);++i){
      if((buff[bp]>>31)  & 0x1){
        skipped_samples= (buff[bp++] & 0x0FFFFFFF) * 2;
        DB(printf("skipped events=%d\n",skipped_samples));
        for(uint j=0; j<skipped_samples; j++) fadc_raw_data[board][ch].emplace_back(0);
        continue;
      }
      
      fadc_raw_data[board][ch].emplace_back(buff[bp] & 0x00003FFF);
      fadc_raw_data[board][ch].emplace_back((buff[bp++] & 0x3FFF0000)>>16);
      
      if(200<i&&i<300) baseline_ave[board][ch]+=((double)fadc_raw_data[board][ch][i])/100.0;
    }
    
    data_bsub.clear();
    for(int i=0;i<(int)fadc_raw_data[board][ch].size();++i){
      if(fadc_raw_data[board][ch][i]==0) data_bsub.push_back(0);
      //      else data_bsub.push_back(abs(fadc_raw_data[board][ch][i]-baseline[board][ch]));
      else data_bsub.push_back(abs(fadc_raw_data[board][ch][i]-baseline_ave[board][ch]));
      if(ADC[board][ch]<data_bsub[i]){
        ADC[board][ch]=data_bsub[i];
        PeakClk[board][ch]=i;
      }
    }
    
    ADC_ch->Fill(16*board+ch,ADC[board][ch]);                         //20240703
    sum[board][ch]=accumulate(data_bsub.begin(),data_bsub.end(),0);
    if(sum[board][ch]<=0) continue;
    
    // data_fil[board][ch]=FIR_filter(data_bsub,LPF_f15_N401_par); // noise filter
    // comment out shaping filter for a while 2021/06/25 22:27 M. Murata
    // data_fil[board][ch]=shaping_filter(data_fil[board][ch],3); // shaper
    
    for(int i=0;i<(int)fadc_raw_data[board][ch].size();i++){
      if(fadc_raw_data[board][ch][i]==0) continue;
      waveform[board][ch]->Fill(i,fadc_raw_data[board][ch][i],1.);
      //ADC_cor[board][ch]=max(ADC_cor[board][ch],data_fil[board][ch][i]);
    }
    //pid parameter
    if(use_Amax){
      data_tra[board][ch] = trapezoidal(data_bsub, 20, 0);
      double max_amax = *std::max_element(data_tra[board][ch].begin(), data_tra[board][ch].end());
      double min_amax = *std::min_element(data_tra[board][ch].begin(), data_tra[board][ch].end());
      double diff_amax = std::abs(max_amax - min_amax);
      if(ADC[board][ch]!=0) Amax[board][ch] = diff_amax / ADC[board][ch];  
      // if(ADC[board][ch]!=0) Amax[board][ch] = diff_amax / ADC_cor[board][ch];  
    }else{
      Amax[board][ch] = -1;
    }
    
    // Energy[board][ch]=(GetSlope(calibpar, board, ch)*ADC_cor[board][ch])+GetInterSection(calibpar, board, ch);
    Energy[board][ch]=(GetSlope(calibpar, board, ch)*ADC[board][ch])+GetInterSection(calibpar, board, ch);
    if(board==0) pid_1st[ch]->Fill(Energy[board][ch], Amax[board][ch]);
    for(int i=0;i<(int)fadc_raw_data[board][ch].size();i++){
      if(fadc_raw_data[board][ch][i]==0) continue;
      if(0.20*ADC[board][ch]<fadc_raw_data[board][ch][i]){
        RiseEdge[board][ch]=i;
        break;
      }
    }   
  }
  break;

  case SIS3820: //sis3820 scaler
  DB(printf("SIS3820 Scaler fadc_raw_data\n"));
  bp =0;
  while(bp < datasize){
    SIS3820_scaler[bp] = buff[bp];
    bp++;
    //cout << "Scaler ch : " << bp << endl;
  }
  break;
  
  case MPV: //mpv TS fadc_raw_data
  high16[board] = (uint64_t)((buff[1] & 0x0000ffff)) << 32;
  low32[board]  = (uint64_t)(buff[0] & 0xffffffff);
  MPV_TS[board] = high16[board] | low32[board];
  MPV_10kclk[board] = buff[2];
  MPV_evtn[board] = buff[3];
  break;
  
}
break;


case RIDF_SCALER: // and RIDF_NCSCALER
case RIDF_CSCALER:
case RIDF_NCSCALER32:
case RIDF_STATUS:
case RIDF_COMMENT:
fin.read((char*)&date, sizeof(date));
fin.read((char*)&scrid, sizeof(scrid));
scrdatasize=blksize-(sizeof(buf_header)+sizeof(date)+sizeof(scrid))/2;
if(cid==RIDF_STATUS){
  fin.read((char*)buff, scrdatasize*2);
  DB(printf("Status Hearder / blkn=%d\n",blkn));
  DB(printf("Status Date =%d\n",date));
  DB(printf("Status ID =%d\n",scrid));
}
else if(cid==RIDF_COMMENT){
  fin.read((char*)comment, scrdatasize*2);
  DB(printf("Comment Hearder / blkn=%d\n",blkn));
  DB(printf("Comment Date =%d\n",date));
  DB(printf("Comment ID =%d\n",scrid));
  if(scrid==1){
    strncpy(run_info.name,comment,100);
    strncpy(run_info.num,&comment[100],100);
    strncpy(run_info.start,&comment[200],20);
    strncpy(run_info.stop,&comment[220],20);
    strncpy(run_info.date,&comment[240],60);
    strncpy(run_info.comment,&comment[300],200);
    
    printf("Run_Name: %s\n",run_info.name);
    printf("Run_Num : %s\n",run_info.num);
    printf("Start   : %s\n",run_info.start);
    printf("Stop    : %s %s\n",run_info.stop,run_info.date);
    printf("Comment : %s\n\n",run_info.comment);
    
    string s_run(run_info.num),s_name(run_info.name);
    fsca_name="sca/"+s_name+s_run+".sca";
    fsca.open(fsca_name);
    fsca << "Run_Name: " << run_info.name << endl;
    fsca << "Run_Num : " << run_info.num << endl;;
    fsca << "Start   : " << run_info.start << endl;
    fsca << "Stop    : " << run_info.stop << " " << run_info.date << endl;
    fsca << "Comment : " << run_info.comment << endl << endl;
    fsca.close();
  }
}
else{
  fin.read((char*)buff, scrdatasize*2);
  DB(printf("Scaler Hearder / blkn=%d\n",blkn));
  DB(printf("Scaler Date =%d\n",date));
  DB(printf("Scaler ID =%d\n",scrid));
  memcpy(scaler,buff,4*32);
}
break;
case RIDF_TIMESTAMP:
segdatasize=blksize-sizeof(buf_header)/2;
fin.read((char*)buff, segdatasize*2);
DB(printf("Timestamp Hearder / blkn=%d\n",blkn));
break;
default:
printf("Error: RIDF Class ID:%d is invalid.\n",cid);
printf("Block Number=%d\n",blkn);
break;
}

if(quit_flag) break;

}

cout << endl;


file->cd();
tree->AutoSave();


for(int i=0;i<N_FADC_BOARD;i++){
  for(int j=0;j<N_CH;j++){
    waveform[i][j]->Write();
  }
}


ADC_ch->Write();

//MADC
for(int i=0;i<N_MADC;i++){
  MADCvsCH[i]->Write();
}
for (int i = 0; i < 16; ++i) {
  pid_1st[i]->Write(); 
}
tree->Write();
file->Close();

for(int i=0;i<16;++i)
printf("Scaler %02d: %10d\tScaler %02d: %10d\n",2*i,scaler[2*i],2*i+1,scaler[2*i+1]);

fsca.open(fsca_name,ios::app);
for(int i=0;i<16;++i)
fsca << "Scaler " << setw(2) << 2*i << ": " << setw(10) << scaler[2*i] << "\tScaler " << setw(2) << 2*i+1 << ": " << setw(10) << scaler[2*i+1] << endl;
if(quit_flag) fsca << "Analysis did not complete." << endl;
fsca.close();

if(quit_flag) printf("\nQuit \n");

// time-mes
  gettimeofday(&eT_all,NULL);
  timersub(&eT_all,&sT_all,&dT_all);
  printf("analyzer    time : %lf us/evt\n",(double)(1e6*dT_all.tv_sec+dT_all.tv_usec)/evtn);
  printf("tree fill   time : %lf us/evt\n",(double)(1e6*dT_fill.tv_sec+dT_fill.tv_usec)/evtn);
  printf("wave filter time : %lf us/evt\n",(double)(1e6*dT_filter.tv_sec+dT_filter.tv_usec)/evtn);
  printf("filter           : %f times/evt\n",(double)filter_cnt/evtn);
  
  return 0;
}
