#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <fcntl.h>

#include <signal.h>
#include <unordered_map>
#include <zlib.h>
#include <bzlib.h>

#include "global.h"
#include "HFC.h"



#define SQR(x)         ((x)*(x))

// real time length of the clock ticks in GRETINA
const double CLOCK_TICK_IN_SECONDS = 1e-8;

// The default width needs to be something 
// long enough that it'd have to be a 
// processing issue for it to be out of place
const double DEFAULT_TIME_WINDOW = 3.0;

// decomposed mode 1 data is type 3
// mode 1 is the more complex tracked data
const int GEB_MODE_1_DECOMP=3;

// This is the size of the payload before the interaction points for mode 3 events
const int GEB_MODE_1_SIZE_BEFORE_INTPT=32;

// used for cropping mode 3 geb events when this is wanted
// this is the size of each interaction
const int GEB_MODE_1_INTPT_SIZE_BYTES=36;

// decomposed mode 2 data is type 1
// mode 2 is the simpler untracked data type
const int GEB_MODE_2_DECOMP=1;

// used for cropping mode 2 geb events when this is wanted
// this is the size of each interaction
const int GEB_MODE_2_INTPT_SIZE_BYTES=24;

// This is the size of the payload before the interaction points for mode 2 events
const int GEB_MODE_2_SIZE_BEFORE_INTPT=80;

struct Fhandle{
  std::string m_fpath = "nil";
  FILE *m_ptr = nullptr;
  gzFile m_gzhandle = nullptr;
  BZFILE *m_bzhandle = nullptr;
  int m_ftype = -1;
  bool m_is_open = false;
  Fhandle(){}

  bool open(const std::string &fpath, int ftype, const std::string &fmode){
    if(ftype > 3 || ftype < 0){
      ftype = 0;
    }
    
    // determine the 
    switch(ftype){
      case 0: // standard file_ptr or stdout
        std::cout << "Open standard file " << fpath << " with mode "<< fmode << std::endl;
        m_ptr = fopen(fpath.c_str(), fmode.c_str());
        m_is_open = m_ptr != nullptr;
        break;
      case 1:
        std::cout << "Open gzip file " << fpath << " with mode "<< fmode << std::endl;
        m_gzhandle = gzopen(fpath.c_str(), fmode.c_str());
        m_is_open = m_gzhandle != nullptr;
        break;
      case 2:
        std::cout << "Open bzip file " << fpath << " with mode "<< fmode << std::endl;
        m_bzhandle = BZ2_bzopen(fpath.c_str(), fmode.c_str());
        m_is_open = m_bzhandle != nullptr;
        break;
      case 3:
        std::cout << "Open stdout file " << fpath << " with mode "<< fmode<< std::endl;
        if(fmode[0] == "r"[0]){
          m_ptr = stdin;
        } else if (fmode[0] == "w"[0]){
          m_ptr = stdout;
        }
      default:
        break;
    }
    if(m_is_open){
      m_ftype = ftype;
      m_fpath = fpath;
    } else {
      std::cout << "Failed to open file " << fpath << std::endl;
      m_fpath = "";
    }
    return m_is_open;
  }


  size_t read(void *__restrict__ buf, int size){
    if(!m_is_open){
      return false;
    }
    //std::cout << "read " << size << " bytes, ftype " << m_ftype << " from " << m_fpath << std::endl;
    size_t bytes_read = 0;
    switch(m_ftype){
      case 0: // standard file_ptr
        bytes_read = fread(buf, 1, size, m_ptr);
        break;
      case 1:
        bytes_read = gzread(m_gzhandle, buf, size);
        break;
      case 2:
        bytes_read = BZ2_bzread(m_bzhandle, buf, size);
        break;
      case 3: // stdin
        bytes_read = fread(buf, 1, size, stdin);
        break;
      default:
        break;
    }
    return bytes_read;
  }

  bool write(void *__restrict__ buf, size_t size){
    if(!m_is_open){
      std::cout << "tried to write to closed file!" << std::endl;
      return false;
    }
    size_t bytes_written = 0;
    //fread(&aGeb, sizeof(struct gebData), 1, in)
    switch(m_ftype){
      case 0: // standard file_ptr
        bytes_written = fwrite(buf, 1, size, m_ptr);
        break;
      case 1:
        bytes_written = gzwrite(m_gzhandle, buf, size);
        break;
      case 2:
        bytes_written = BZ2_bzwrite(m_bzhandle, buf, size);
        break;
      case 3: //stdout
        bytes_written = fwrite(buf, 1, size, m_ptr);
        break;
      default:
        break;
    }
    return bytes_written ==  size;
  }

  void close(){
    // nothing to do
    if(!m_is_open){return;}
    size_t bytes_written = 0;
    //fread(&aGeb, sizeof(struct gebData), 1, in)
    switch(m_ftype){
      case 0: // standard file_ptr
        fclose(m_ptr);
        break;
      case 1:
        gzclose(m_gzhandle);
        break;
      case 2:
        BZ2_bzclose(m_bzhandle);
        break;
      case 3: //stdout
        // nothing to do
        break;
      default:
        break;
    }
    m_is_open = false;
  }
};


int gotsignal;
void breakhandler(int dummy) {
  gotsignal = 1;
}

struct Mode3event
{
  int length;
  int board_id;
  int chn_id;
  int module;
  long long LED_ts;
  int en;
  bool en_sign;
  int pileup;
  long long CFD_ts;
  int CFD_1;
  int CFD_2;
  int trace_len;
  INT16 trace[8192];
};

void swapbytes(char* a, char *b)
{
  char tmp=*a;
  *a=*b;
  *b=tmp;
}

// Mode 3 data is high endian format
void HEtoLE(char* cBuf, int bytes) {
  for(int i=0; i<bytes; i+=2) 
    swapbytes((cBuf+i), (cBuf+ i+1));
} 

void Mode3Event(char* cBuf, int len, Mode3event* mode3) {
  // Welcome in the 16 bit world
  UINT16* wBuf= (UINT16*)cBuf;
  
  // digitizer saves in high endian format
  // we're low endian
  HEtoLE(cBuf, len);
  
  // length now in units of 16bit words
  len/=2;
  
  // 1st & 2nd word are 0xaaaa
  if((*wBuf != 0xaaaa) && (*(wBuf+1) != 0xaaaa)) {
    std::cerr << "0xAAAA header missing" << endl;
    return;
  }
  wBuf+=2;

  // 3rd word
  // Digitizer reports length in 32bit units
  // we convert in 16bit. Furthermore the length
  // doesn't account for the two 0xAAAA words
  
  mode3->length = (*wBuf & 0x07ff) * 2 + 2;
  if(mode3->length != len) {
    std::cerr << "inconsistent mode3 buffer length "
              << "Geb Hdr: " << len << " wrds "
              << "Mode2: " << mode3->length << " wrds"
              <<endl;
    return;
  }
  
  // also board id encoded (=GA ?)
  mode3->board_id = *wBuf >> 11;
  wBuf++;
  
  // 4th word
  mode3->chn_id = *wBuf & 0x000f;
  mode3->module = *wBuf >> 4;
  wBuf++;

  // 5th, 6th and 8th word LED timestamp
  // 5th 'middle', 6th 'lowest', 8th 'highest'

  mode3->LED_ts = ((long long) *(wBuf+3)) << 32;
  mode3->LED_ts += ((long long) *(wBuf+0)) << 16;
  mode3->LED_ts += ((long long) *(wBuf+1)) << 0 ;
  wBuf+=2; //point 7th
  
  // 7th is low 16bit energy
  // 10th upper 8bit, 9th bit sign
  mode3->en = (int) *(wBuf+3) & 0x00ff;
  mode3->en = mode3->en << 16;
  mode3->en += *wBuf;
  
  mode3->en_sign = *(wBuf+3) & 0x0100;
  mode3->pileup  = (*(wBuf+3) & 0x8000) >> 15;
  wBuf+=2; //point 9th

  // 9th, 11th and 12th word CFD ts
  // 9th 'lower, 11th 'highest', 12th 'middle'
  mode3->CFD_ts = ((long long) *(wBuf+2)) << 32;
  mode3->CFD_ts += ((long long) *(wBuf+3)) << 16;
  mode3->CFD_ts += ((long long) *(wBuf+0)) << 0 ;
  wBuf+=4; //point 13th
  
  // 13th, 14th CFD point 1
  mode3->CFD_1 = (int) *(wBuf+1) << 16;
  mode3->CFD_1 += (int) *wBuf;
  wBuf+=2; //point 15th
  
  // 15th, 16th CFD point 2
  mode3->CFD_2 = (int) *(wBuf+1) << 16;
  mode3->CFD_2 += (int) *wBuf;
  wBuf+=2; //point 17th
  
  // wBuf points at 1st trace element now
  mode3->trace_len = mode3->length - 16; 
  
  for(int i=0; i<mode3->trace_len/2; i++) {
#define OFFSET 512;
    mode3->trace[2*i+1]  =-(INT16)(*(wBuf+1)) + OFFSET;
    mode3->trace[2*i+0]=-(INT16)(*(wBuf+0)) + OFFSET;
    wBuf+=2;
  }
  
  std::cerr << hex
       << " LED: 0x" << mode3->LED_ts
       << " CFD: 0x" << mode3->CFD_ts
       << dec << endl;
}

void BrowseData(gebData header) {
  std::cerr << "type:" << header.type 
       << " len: " << header.length
       << " ts: 0x" << hex << header.timestamp << dec
       << endl;
}


int HFC_mode3(BYTE* cBuf, HFC* hfc_list) {
  /* Return value: processed data in bytes */

  long long mode3_ts;
  int mode3_len;
 
  // 15th and 16th byte is ts high (and endian)
  mode3_ts = ((long long) cBuf[14]) << 40;
  mode3_ts += ((long long) cBuf[15]) << 32;
  // 9th, 10th ts middle 
  mode3_ts += ((long long) cBuf[8]) << 24;
  mode3_ts += ((long long) cBuf[9]) << 16;
  // 11th, 12th ts 'low'
  mode3_ts += ((long long) cBuf[10]) << 8;
  mode3_ts += ((long long) cBuf[11]);

  // 5th, 6th is length, in 32bit units
  mode3_len = ((int)(cBuf[4])) << 8;
  mode3_len += ((int)cBuf[5]);
  mode3_len &= 0x7ff;
  mode3_len *= 4; // convert into bytes

  hfc_list->add(mode3_ts, 2, mode3_len+4, cBuf);
  
  return (mode3_len + 4); // 0xaaaa 0xaaaa not counted in mode3_len
}

void CropGeb(HFC_item *evt){
  
  if (evt->geb.type == GEB_MODE_2_DECOMP){
    int num_int_pts = *reinterpret_cast<int *>((char *)evt->data + 8);
    //std::cout << "Num int pts " << num_int_pts << std::endl;
    size_t new_payload_length =  GEB_MODE_2_SIZE_BEFORE_INTPT + (std::min(16, num_int_pts) * GEB_MODE_2_INTPT_SIZE_BYTES);
    // chop off everything after the payload that's there
    evt->data = (BYTE *)realloc(evt->data, new_payload_length);
    evt->geb.length = new_payload_length;
  } else if (evt->geb.type == GEB_MODE_1_DECOMP){
    int num_det = *reinterpret_cast<int *>((char *)evt->data + 4);
    //std::cout << "Num det " << num_det << std::endl;
    size_t new_payload_length = GEB_MODE_1_SIZE_BEFORE_INTPT + (std::min(30, num_det) * GEB_MODE_1_INTPT_SIZE_BYTES);
    // chop off everything after the payload that's there
    evt->data = (BYTE *)realloc(evt->data, new_payload_length);
    evt->geb.length = new_payload_length;
  }
  // can't effectively crop other event types unless we know something about their structure
}


void PrintUsageString(char** argv){
  std::cerr << "Usage: " << argv[0] << " [-o outputfile.dat[.gz or .bz] [-c] [-t time window in seconds ] inputfile.dat[.gz or .bz]]" << std::endl
    << "time sorts GEB event built files" << std::endl
    << "\t-c flag: GRETINA events will be cropped to remove zero padding" << std::endl
    << "\t-t arg: must be a positive number, ex [ -t 1.01 ]" << std::endl
    << "\t-o arg: must be a file with the .dat, .dat.gz or .dat.bz extensions" << std::endl;
}

int main(int argc, char** argv) {

  gotsignal = 0;
  signal(SIGINT, breakhandler);
  signal(SIGPIPE, breakhandler);

  if(argc==1) {
    PrintUsageString(argv);
    exit(0);
  }
  std::unordered_map<int, std::string> type_to_str = {
    {1,  "Decomposed Gretina Data"},
    {2,  "Raw Gretina Data"},
    {3,  "Tracked Gretina Data"},
    {4,  "BGS Raw Gretina Data"},
    {5,  "S800 Raw Data"},
    {6,  "NSCL Non Event Data"},
    {7,  "Gretina Scaler Data"},
    {8,  "Bank 29 Raw Data"},
    {9,  "S800 processed data"},
    {10, "Timestamped NSCL Non event data"},
    {11, "Gretina GEANT 4 simulation Data"},
    {12, "Chico raw data"},
    {13, "Superstitious Mystery Data"},
    {14, "Digital Gammasphere Data"},
    {15, "Digital Gammasphere Trigger Data"},
    {16, "Digital FMA Data"},
    {17, "Phoswich Wall"},
    {18, "Phoswich Wall Auxilliary"},
    {19, "GODDESSS"},
  };
  std::vector<int> types_seen(51);
  bool pipeflag = false;
  bool zipflag = false;
  bool bzipflag = false;
  Fhandle in;
  Fhandle out;

  string filename = "NONE";
  string outfname = "HFC.dat";
  int input_ftype = 0;
  // how far separated in time the latest 
  // events can be before we dump them to disk
  double time_window = DEFAULT_TIME_WINDOW;
  bool crop_geb = false;
  for(int arg = 1; arg < argc; arg++) {
    std::string strval = std::string(argv[arg]);
    if(!strval.compare("-h") || !strval.compare("--help")){
      PrintUsageString(argv);
      exit(1);
    } else if (!strval.compare("-p")) {
      input_ftype = 3; // stdout
      pipeflag = true;
    } else if (!strval.compare("-c")){
      crop_geb = true;
    } else if (!strval.compare("-t")){
      arg++;
      if(arg >= argc){ break;}
      try{
        time_window = std::stod(argv[arg]);
      } catch(std::exception &e){
        std::cout << "Failed to parse '" << argv[arg] << "' as a double, using default window of " << DEFAULT_TIME_WINDOW  << "s" << std::endl;
        time_window = DEFAULT_TIME_WINDOW;
      }
    } else if (!strval.compare("-o")) {
      arg++;
      if(arg >= argc){ break; }
      outfname = argv[arg];
      std::cout << "Using output: " << outfname << std::endl;
    } else if (!filename.compare("NONE")) {
      // the first argument without
      // a flag becomes the output file
      // you should it be the last
      filename = argv[arg];
    }
  }
  if(crop_geb){
    std::cout << "Cropping geb events!" << std::endl;
  }
  std::cout << "Using Time window " << time_window  << "s" << std::endl;
  int outf_type = 0;
  int inf_type = 0;
  // handling the extension here instead of from the argument flags
  size_t  outf_extension_pos = outfname.rfind(".");
  if(outf_extension_pos == outfname.npos){
    if(!pipeflag){
      std::cerr << "Failed to extract file extension from output file name '" << outfname << "'" << std::endl;
  if(inf_extension_pos == filename.npos){
    if(!pipeflag){
      std::cerr << "Failed to extract file extension from input file name '" << filename << "'" << std::endl;
    }
    exit(1);
  }
    }
    exit(1);
  }
  std::string outf_extension = outfname.substr(outf_extension_pos);
  
  size_t  inf_extension_pos = filename.rfind(".");
  if(inf_extension_pos == filename.npos){
    if(!pipeflag){
      std::cerr << "Failed to extract file extension from input file name '" << filename << "'" << std::endl;
    }
    exit(1);
  }
  std::string inf_extension = filename.substr(inf_extension_pos);
  
  std::vector<std::string> allowed_file_extensions = {
    ".dat", ".dat.bz", ".dat.gz"
  };
  std::unordered_map<std::string, int> extension_to_ftype = {
    {".dat", 0}, {".gz", 1}, {".bz", 2}
  };
  if(extension_to_ftype.find(outf_extension) != extension_to_ftype.end()){
    outf_type = extension_to_ftype.at(outf_extension);
  } else {
    if(!pipeflag){
      std::cerr << "Output file extension not allowed! Allowed extensions are:" << std::endl;
      for(const auto &i: allowed_file_extensions){
        std::cerr << i<< " ";
      }
      std::cerr << std::endl;
    }
    exit(1);
  }
  if(extension_to_ftype.find(inf_extension) != extension_to_ftype.end()){
    inf_type = extension_to_ftype.at(inf_extension);
  } else {
    if(!pipeflag){
      std::cerr << "Input file extension not allowed! Allowed extensions are:" << std::endl;
      for(const auto &i: allowed_file_extensions){
        std::cerr << i << " ";
      }
      std::cerr << std::endl;
    }
    exit(1);
  }
  // pipe to stdout
  if(pipeflag){ 
    outf_type = 3;
  }
  if (outfname.size() == 0){
    outfname = "HFC.dat";
  }
  in.open(filename,inf_type,"rb");
  // if it's piped, it ignores the other info
  out.open(outfname,outf_type,"wb");
  if(!in.m_is_open){
    if(!pipeflag){
      std::cerr << "Failed to open input file!" << std::endl;
    }
    exit(1);
  }
  if(!out.m_is_open){
    if(!pipeflag){
      std::cerr << "Failed to open output file!" << std::endl;
    }
    exit(1);
  }
  long long totread = 0;
  size_t read;
  int EvtCount=0;
  BYTE cBuf[8*16382];
  gebData aGeb;
  //HFC hfc_list(50*8192, out);
  // 972: strange mode 2 with mem depth 40*8192 needed

  // how many bytes are waiting to write
  long long waiting_writes = 0;

  std::priority_queue<HFC_item *, std::vector<HFC_item *>, MinHeapSortHFCptr> pq;
  bool success=true;
  unsigned long long badevt = 0;
  
  double t_timestamp_in_seconds;
  double last_write_in_seconds;
  bool none_written = true;
  double first_timestamp = -1e20;
  
  long long last_written_timestamp = 0;
  int       last_written_type = 0;
  unsigned long long timestamps_out_of_order = 0;
  unsigned long long total_written = 0;
  unsigned long long type_19_ooo = 0;
  double tdiff = 0;
  unsigned long long max_write_queue_size = 10000000;
  float write_fraction = 0.1;
  std::unordered_map<int, unsigned long long> by_type{
    {1, 0}, {8, 0}, {19, 0}
  };
  while (in.read(&aGeb, sizeof(gebData)) && !gotsignal) {
    if(!pipeflag && (aGeb.type < 1 || aGeb.type > 50 || aGeb.length > 8192)){
      std::cout << "bad evt with type " << aGeb.type << " length: " << aGeb.length << std::endl; 
      badevt++;
      continue;
    }
    read = in.read(cBuf, aGeb.length);
    types_seen[aGeb.type] = types_seen[aGeb.type]+1;
    if(aGeb.type == 13){continue;}
    if(aGeb.timestamp < last_written_timestamp){
      timestamps_out_of_order++;
      if(aGeb.type == 19){
        type_19_ooo++;
      }
    }
    // ORRUBA gets bad timestamps some fraction of the time
    if(aGeb.type != 19 && aGeb.type != 8){
      // if we're horribly out of order, we don't want to
      // write out prematurely
      if(!none_written){
        tdiff = aGeb.timestamp * CLOCK_TICK_IN_SECONDS - last_write_in_seconds;
      } else if (first_timestamp < -1e10) {
        first_timestamp = aGeb.timestamp * CLOCK_TICK_IN_SECONDS;
      } else {
        // might have issues if the first time stamp is bad
        tdiff = aGeb.timestamp * CLOCK_TICK_IN_SECONDS - first_timestamp;
      }
    }
    

    
    totread += read + sizeof(struct gebData);
    if (read != aGeb.length) {
      if (!pipeflag) {
        std::cerr << aGeb.length << " bytes expected but"
        << read << " bytes read. Bailing out"
        << std::endl;
        std::cerr.flush();
      }
      break;
    }
    EvtCount++;
    if((EvtCount % 20000)==0 && !pipeflag) {
      std::cerr << "Event " << EvtCount
      << " total read:" << totread/1000000 << " Mb " 
      << " Out of order: " << timestamps_out_of_order 
      << " tdiff: " << tdiff << "s " 
      << " pqlen: " << pq.size();
      for(const auto &i : by_type){
        std::cerr << " {" << i.first << ", " << (double)i.second / (double)pq.size() << "} ";
      }
      std::cerr << "\r";
      std::cerr.flush();
    }
    HFC_item* hfc;
    hfc = (HFC_item*) new HFC_item;
    hfc->geb = aGeb;

    hfc->data = (BYTE*) new BYTE [hfc->geb.length * sizeof(BYTE)];
    memcpy(hfc->data, cBuf, hfc->geb.length * sizeof(BYTE));
    
    if(crop_geb){
      CropGeb(hfc);
    }
    waiting_writes += (hfc->geb.length) + 16;
    by_type[hfc->geb.type] += 1;
    pq.push(hfc);
    if( waiting_writes < max_write_queue_size ){ continue; }
    if(tdiff < time_window * 0.8){
      max_write_queue_size *= 1.5;
      // keep the write queue from going above 2GB
      max_write_queue_size = std::min(2000000000ull, max_write_queue_size);
      continue;
    }

    size_t start_size = pq.size() / 2;
    none_written = false;
    long num_writes = 0;
    unsigned long long stop_thresh = (unsigned long long)((1.0 - write_fraction) * max_write_queue_size);
    if(tdiff > time_window * 2.0){
      max_write_queue_size /= 1.5;
      // keep the write queue from going below 10MB
      max_write_queue_size = std::max(10000000ull, max_write_queue_size);
    }
    while(waiting_writes > stop_thresh){
      HFC_item *item = pq.top();
      pq.pop();
      waiting_writes -= (item->geb.length) + 16;
      total_written  += (item->geb.length) + 16;
      out.write(&(item->geb), sizeof(gebData));
      out.write(item->data, item->geb.length);
      by_type[item->geb.type] -= 1;
      if(item->geb.type != 19 && item->geb.type != 8){
        last_written_timestamp = item->geb.timestamp;
        last_written_type = item->geb.type;
        last_write_in_seconds  = last_written_timestamp * CLOCK_TICK_IN_SECONDS;
        if(none_written)
        tdiff = last_write_in_seconds - t_timestamp_in_seconds;
      }
      delete item->data;
      delete item;
      //std::cout << last_write_in_seconds << std::endl;
      num_writes++;
      none_written = false;
    }


    continue;
#if(0)
    if(!hfc_list.add(aGeb, cBuf)){
      if (!pipeflag) {
      std::cerr << "HFC: adding event in HFC failed"
            << endl;
      }
    }
    continue;
    std::cerr << "Event:" << EvtCount 
   << " #data:" << read
   << " geb:" << sizeof(struct gebData)
   << " total bytes read: " << totread
   << " (0x" << hex << totread << dec << ")"
   << endl;

    
    switch(aGeb.type) {
      case 1: // Mode 2, so far always GEB, 1 event, GEB, 1 event,....
        if(!hfc_list.add(aGeb, cBuf) && success) {
            success = false;
            if (!pipeflag) {
              std::cerr << "HFC: adding event in HFC failed"
                   << endl;
              }
          }
        break;
        
      case 2: // Mode 3 (raw)
      {
        int nBytes = aGeb.length;
        BYTE *data;
        data=cBuf;
        while(nBytes) {
          int nread;
          nread = HFC_mode3(data, &hfc_list);

          data+=nread;
          nBytes -=nread;

          if(nBytes < 0) {
            if (!pipeflag) {
              std::cerr << "HFC: nBytes negative!!"
             << endl;
            }
            exit(0);
            break;
          }
        }
      }
      break;
      
      case 3: // Mode 1
        if(!hfc_list.add(aGeb, cBuf) && success) {
          success = false;
          if (!pipeflag) {
            std::cerr << "HFC: adding event in HFC failed"
                 << endl;
          }
        }
        break;
      
      case 4: // BGS
        break;
      
      case 5: // S800 event
        if(!hfc_list.add(aGeb, cBuf) && success) {
          success = false;
          if (!pipeflag) {
            std::cerr << "HFC: adding event in HFC failed"
                 << endl;
          }
        }
        break;
      
      case 6: // S800 scaler etc.
        if(!hfc_list.add(aGeb, cBuf) && success) {
          success = false;
          if (!pipeflag) {
            std::cerr << "HFC: adding event in HFC failed"
                 << endl;
          }
        }
        break;
      
    case 7: // GRETINA scaler data - not yet implemented
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }  
      break;
      
    case 8: // card 29
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;
      
    case 9: // S800 physics event
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;
      
    case 10: // S800 timestamped non-event (i.e. scaler) data
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 11: // Simulated GRETINA data
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 0x2B: // Contrived clover for coincidence
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 12: // CHICO
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 16: // DFMA
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 17: // PHOSWALL
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 19: // GODDESS
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;


    case 18: // PHOSWALLAUX
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;

    case 21: // LENDA
      if(!hfc_list.add(aGeb, cBuf) && success) {
        success = false;
        if (!pipeflag) {
          std::cerr << "HFC: adding event in HFC failed"
               << endl;
        }
      }
      break;
      
    default:
      {
        if (!pipeflag) {
          std::cerr << "HFC: Unknown packet type " << aGeb.type
               << " ... HFC: skipping that one" << endl;
        }
      }
      break;

    }
    #endif  
  }
  
  if (!pipeflag) {
    std::cerr << "HFC: calling flush" << endl; std::cerr.flush();
  }
  while(pq.size() > 0){
    HFC_item *item = pq.top();
    if(item == nullptr){
      continue;
    }
    pq.pop();
    if(item->data != nullptr){
      out.write(&(item->geb), sizeof(gebData));
      out.write(item->data, item->geb.length);
      delete item->data;
    }
    delete item;
  }
  //hfc_list.flush();
  //hfc_list.printstatus();
  
  if (!pipeflag) {
    std::cerr << "HFC: closing files" << endl; std::cerr.flush();
  }
  in.close();
  out.close();

  if (!pipeflag) {
      for(int i = 0; i < types_seen.size(); i++){
    if(types_seen[i] == 0){continue;}
    if(type_to_str.find(i) == type_to_str.end()){
      std::cout << "Saw " << types_seen[i] << " type " << i << " unknown type event!" << std::endl;
    } else {
      std::cout << "Saw " << types_seen[i] << " type " << i << " " << type_to_str.at(i) << " events!" << std::endl;
    }
  }
    std::cerr << "HFC: done" << endl; std::cerr.flush(); 
    if(timestamps_out_of_order > 0){
      std::cout << "Out of order timestamps: " << timestamps_out_of_order << " of " << EvtCount << std::endl;
      std::cout << "Type 19 out of order: " << type_19_ooo << std::endl;
    }
    double space_ratio = 1.0 - ((double)(total_written) / (double)(totread));
    if(crop_geb){
      std::cout << "Wrote " << total_written / 1000000 << " MB and Read " <<  totread/1000000 << " MB Saved " << space_ratio * 100.0 << "% by cropping" << std::endl;
    }
  }
}

	 
