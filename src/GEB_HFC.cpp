#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <fcntl.h>

#include <signal.h>
#include<unordered_map>
#include <zlib.h>
#include <bzlib.h>

#include "global.h"
#include "HFC.h"



#define SQR(x)         ((x)*(x))

using namespace std;

struct Fhandle{
  std::string m_fpath;
  FILE *m_ptr;
  gzFile m_gzhandle;
  BZFILE *m_bzhandle;
  int m_ftype;
  bool m_is_open;
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


  size_t read(void *buf, int size){
    if(!m_is_open){
      return false;
    }
    //std::cout << "read " << size << " bytes, ftype " << m_ftype << " from " << m_fpath << std::endl;
    size_t bytes_read = 0;
    switch(m_ftype){
      case 0: // standard file_ptr
        bytes_read = fread(&buf, size, 1, m_ptr);
        break;
      case 1:
        bytes_read = gzread(m_gzhandle, buf, size);
        break;
      case 2:
        bytes_read = BZ2_bzread(m_bzhandle, buf, size);
        break;
      case 3: // stdin
        bytes_read = fread(&buf, size, 1, stdin);
        break;
      default:
        break;
    }
    return bytes_read;
  }

  bool write(void *buf, size_t size){
    if(!m_is_open){
      std::cout << "tried to write to closed file!" << std::endl;
      return false;
    }
    size_t bytes_written = 0;
    //fread(&aGeb, sizeof(struct gebData), 1, in)
    switch(m_ftype){
      case 0: // standard file_ptr
        bytes_written = fwrite(&buf, size, 1, m_ptr);
        break;
      case 1:
        bytes_written = gzwrite(m_gzhandle, buf, size);
        break;
      case 2:
        bytes_written = BZ2_bzwrite(m_bzhandle, buf, size);
        break;
      case 3: //stdout
        bytes_written = fwrite(&buf, size, 1, m_ptr);
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

int main(int argc, char** argv) {

  gotsignal = 0;
  signal(SIGINT, breakhandler);
  signal(SIGPIPE, breakhandler);

  if(argc==1) {
    std::cerr << argv[0] << " <flag: -p (pipeout) or -z (.gz input file)> <Input file>" << endl
   << "brings GRETINA Mode3 event file" << endl
   << "in proper sequence" << endl;
    exit(0);
  }
  
  bool pipeflag = false;
  bool zipflag = false;
  bool bzipflag = false;
  Fhandle in;
  Fhandle out;

  string filename;
  string outfname = "HFC.dat";
  int input_ftype = 0;
  std::cout << argc << std::endl;
  for(int arg = 1; arg < argc; arg++) {
    std::string strval = std::string(argv[arg]);
    if (!strval.compare("-p")) {
      input_ftype = 3; // stdout
      pipeflag = true;
    } else if (!strval.compare("-z")) {
      input_ftype = 
      zipflag = true;
    } else if (!strval.compare("-bz")) {
      bzipflag = true;
    } else if (!strval.compare("-o")) {
      arg++;
      if(arg >= argc){
        break;
      }
      outfname = argv[arg];
      std::cout << "Using output: " << outfname << std::endl;
    } else {
      filename = argv[arg];
    }
  }
  int outf_type = 0;
  int inf_type = 0;
  // handling the extension here instead of from the argument flags
  size_t  outf_extension_pos = outfname.rfind(".");
  std::string outf_extension = outfname.substr(outf_extension_pos);
  
  size_t  inf_extension_pos = filename.rfind(".");
  std::string inf_extension = filename.substr(inf_extension_pos);
  
  std::unordered_map<std::string, int> extension_to_ftype = {
    {".gz", 1}, {".bz", 2} 
  };
  if(extension_to_ftype.find(outf_extension) != extension_to_ftype.end()){
    outf_type = extension_to_ftype.at(outf_extension);
  }
  if(extension_to_ftype.find(inf_extension) != extension_to_ftype.end()){
    inf_type = extension_to_ftype.at(inf_extension);
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
  if(!out.m_is_open){
    exit(1);
  }
  long long totread = 0;
  int read;
  int EvtCount=0;
  BYTE cBuf[8*16382];
  gebData aGeb;
  //HFC hfc_list(50*8192, out);
  // 972: strange mode 2 with mem depth 40*8192 needed
  // how many bytes are waiting to write
  long long waiting_writes = 0;

  std::priority_queue<HFC_item *, std::vector<HFC_item *>, MinHeapSortHFCptr> pq;
  bool success=true;
  while (in.read(&aGeb, sizeof(struct gebData)) && !gotsignal) {

    read = in.read(cBuf, aGeb.length);
    totread += read + sizeof(struct gebData);
    if (read != aGeb.length) {
      if (!pipeflag) {
        std::cerr << aGeb.length << " bytes expected but"
             << read << " bytes read. Bailing out"
             << endl;
        std::cerr.flush();
      }
      break;
    }
    EvtCount++;
    if((EvtCount % 20000)==0 && !pipeflag) {
      std::cerr << "Event " << EvtCount
        << " read:" << read
        << " total read:" << totread/1000000
        << " Mb \r";
      std::cerr.flush();
    }
    // flush the first 99MB of data
    if(waiting_writes > 100000000){
      while(waiting_writes > 1000000){
        HFC_item *item = pq.top();
        pq.pop();
        waiting_writes -= (item->geb.length) + 16;
        out.write(&(item->geb), sizeof(gebData));
        out.write(item->data, item->geb.length);
        delete item->data;
        delete item;
      }
    }
    HFC_item* hfc;
    hfc = (HFC_item*) new HFC_item;
    hfc->geb = aGeb;
    waiting_writes += (aGeb.length) + 16;

    hfc->data = (BYTE*) new BYTE [hfc->geb.length * sizeof(BYTE)];
    memcpy(hfc->data, cBuf, hfc->geb.length * sizeof(BYTE));
    pq.push(hfc);

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
    std::cerr << "HFC: done" << endl; std::cerr.flush(); 
  }
}

	 
