#include "HFC.h"
#include "stdio.h"
#include "string.h"


#define HFC_DEFAULTNUM 8192

HFC::HFC() {
  init(HFC_DEFAULTNUM, NULL);
}

HFC::HFC(int num) {
  init(num, NULL);
}

HFC::HFC(FILE* out) {
  init(HFC_DEFAULTNUM, out);
}

HFC::HFC(int num, FILE* out) {
  init(num, out);
}

void HFC::init(int num, FILE* out) {
  m_evt=0;
  m_file = out;

  if(num > 200)
    m_memdepth = num;
  else
    m_memdepth = 200;

  m_discarded=0;
}

// We assume that the latest item to be added is actually
// pretty late according its timestamp.

bool HFC::insert(HFC_item* hfc) {
  pq.push(hfc);
  return true;
  #define HFCITEMISDEEP 100
  int cts=0;

  list<HFC_item*>::iterator HFC_it = m_HFClist.end();
  list<HFC_item*>::iterator HFC_it2 = m_HFClist.begin();

  HFC_it--;

  while(HFC_it != m_HFClist.begin() &&
	(*HFC_it)->geb.timestamp >=
	hfc->geb.timestamp ) {
    HFC_it--;
    cts++;
    if((cts == HFCITEMISDEEP) &&
       (m_HFClast_it != m_HFClist.begin()))
      {
	// It can happen that a chunck of data comes
	// with very 'old' timestamps. For first item of
	// those chunks we need to dig through the list,
	// but for the other the location in the list
	// where this one was put might be a much better
	// starting point.
	cts++;
	if((*m_HFClast_it)->geb.timestamp < hfc->geb.timestamp) {
	  if((hfc->geb.timestamp-(*m_HFClast_it)->geb.timestamp) <
	     ((*HFC_it)->geb.timestamp - hfc->geb.timestamp)) {
	    // our iterator for last event is closer to current
	    // event AND last iterator TS is < hfc.TS.
	    // We need to go up in list.
	    HFC_it=m_HFClast_it;
	    while((*HFC_it)->geb.timestamp <
		  hfc->geb.timestamp) {
	      HFC_it++;
	      cts++;
	    }
	    HFC_it--; // as we do HFC_it++ after loop.
	    break;
	  } else {
	    // no luck this time, we need to iterate through
	    // the hole thing (list)
	  }
	} else {
	  if(((*m_HFClast_it)->geb.timestamp - hfc->geb.timestamp) <
	     ((*HFC_it)->geb.timestamp - hfc->geb.timestamp)) {
	    // again last event's iterator is closer and its
	    // TS is larger. we can just move on with that one.
	    HFC_it=m_HFClast_it;
	  } else {
	    // no luck.....
	  }
	}
      }
  } /* while */

  HFC_it++;

  if (((*HFC_it2)->geb.timestamp) > hfc->geb.timestamp) {
    m_HFClist.push_front(hfc);
  } else {
    m_HFClast_it=m_HFClist.insert(HFC_it, hfc);
  }

  // m_HFClast_it=m_HFClist.insert(HFC_it, hfc);

  return true;
}


bool HFC::add(long long TS, int type, int length, BYTE* data) {
  gebData geb;
  geb.timestamp = TS;
  geb.type = type;
  geb.length = length;

  return add(geb, data);
}

bool HFC::add(gebData aGeb, BYTE* data) {
  m_evt++;

  // first we make our 'private' copy
  HFC_item* hfc;
  hfc = (HFC_item*) new HFC_item;
  hfc->geb = aGeb;
  waiting_writes += (aGeb.length) + 16;

  hfc->data = (BYTE*) new BYTE [hfc->geb.length * sizeof(BYTE)];
  memcpy(hfc->data, data, hfc->geb.length * sizeof(BYTE));
  // if we're waiting for more than 10MB of writes, flush
  // till 1MB waiting
  if(waiting_writes > 10000000){
    while(waiting_writes > 1000000){
      HFC_item *item = pq.top();
      pq.pop();
      waiting_writes -= (item->geb.length) + 16;
      writeItem(item);
      delete item->data;
      delete item;
    }
  }
  pq.push(hfc);
  return true;
  // now storing the pointer in HFC_list

  // You DON'T want to use
  // (m_HFClist.size() >= m_memdepth)
  // as it performs O(n)

  // cout << hfc->geb.timestamp << endl;

  if(m_evt >= m_memdepth) {
    return addToFullList(hfc);
  } else if(m_HFClist.empty()) {
    m_HFClist.push_back(hfc);
    return true;
  } else {
    return insert(hfc);
  }
}

bool HFC::addToFullList(HFC_item* hfc) {
  list<HFC_item*>::iterator HFC_it = m_HFClist.begin();

  if(((*HFC_it)->geb).timestamp > (hfc->geb).timestamp) {
    m_discarded++;

    // cout << "Start of list " << ((*HFC_it)->geb).timestamp;
    // HFC_it = m_HFClist.end();
    // cout << ", end of list " << ((*HFC_it)->geb).timestamp << endl;
    // HFC_it = m_HFClist.begin();

    if(0) {
      cerr << "HFC::addToFullList has major problem"
	   << endl
	   << "current timestamp item "
	   << dec << hfc->geb.timestamp << endl
	   << "oldest timestamp in list "
	   << (*HFC_it)->geb.timestamp << dec << endl
	   << "discarding this item, you should increase memory depth"
	   << endl << endl;
      cin.get();
    }

    return false;
  }



  // okay, write 'oldest' event from list and get rid of it
  writeItem(*HFC_it);
  // destroy data in our private copy
  delete (*HFC_it)->data;
  delete (*HFC_it);
  // and remove the pointer from list
  HFC_it = m_HFClist.erase(HFC_it);

  // and now we add the new item
  insert(hfc);

  return true;
}

bool HFC::writeItem(HFC_item* hfc) {
  if(m_file) {
    fwrite(&(hfc->geb), sizeof(gebData), 1, m_file);
    fwrite(hfc->data, sizeof(BYTE), hfc->geb.length, m_file);
  }

  return true;
}

void HFC::flush() {
  //list<HFC_item*>::iterator HFC_it = m_HFClist.begin();
  while(pq.size() > 0){
    HFC_item *item = pq.top();
    if(item == nullptr){
      continue;
    }
    pq.pop();
    if(item->data != nullptr){
      writeItem(item);
      delete item->data;
    }
    delete item;
  }
  fflush(stdout);
  // while(HFC_it != m_HFClist.end()) {
  //   writeItem(*HFC_it);
  //   delete (*HFC_it)->data;
  //   delete *HFC_it;
  //   HFC_it = m_HFClist.erase(HFC_it);
  // }
}

void HFC::printstatus() {
  cerr << "\t\tStatus of HFC object:"
       << endl
       << "\t\tEvent memory depth: " << m_memdepth
       << endl
       << "\t\tEvents processed:   " << m_evt
       << endl;
  if(m_discarded)
    cerr << "\t\tEvents discarded:   " << m_discarded
    	 << "  (increase mem depth!)"
    	 << endl;
}
