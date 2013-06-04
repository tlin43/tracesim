#include <errno.h> 
#include <string>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <algorithm>

using namespace std;

class Alloy_Cache;
class Regular_Cache;
class virtual_set_record;

unsigned int CACHE_SIZE;
unsigned int CACHE_ASSOC;
unsigned int CORE_NUM;
unsigned int SCHEME;
std::string TRACE_FILE;
std::string OUTPUT_FILE;
Alloy_Cache * alloycache = NULL;
Regular_Cache * dramcache = NULL;
virtual_set_record * virtualsets = NULL;
unsigned int VIRTUAL_SIZE;

#define DIST_BIN_SIZE 16
#define REUSE_DIST_UNIT 5
#define ACCESS_DIST_UNIT 131072
#define DOA_DIST_UNIT (4096)
#define SHORTHIT_DIST_UNIT 4096

class cache_block_t
{
public:  
  unsigned int tag; // Here tag = address>>6
  unsigned int addr;
  
  bool isValid;
  bool isDirty;
  
  unsigned int hit_cnt;
  
  long long last_hit_virtualset_cnt;
  
  cache_block_t()
  {
    tag = 0;
    addr = 0;
    isValid = false;
    isDirty = false;
    hit_cnt = 0; 
    last_hit_virtualset_cnt = 0;
  }
  cache_block_t & operator=(const cache_block_t & in)
  {
    tag = in.tag;
    addr = in.addr;
    isValid = in.isValid;
    isDirty = in.isDirty;
    
    hit_cnt = in.hit_cnt;
    last_hit_virtualset_cnt = in.last_hit_virtualset_cnt;
    
    return *this;
  }
};


//==========================================================================
// Alloy Cache
class Alloy_Cache
{
public:  
  unsigned int num_sets;
  unsigned int num_rows; // assume 28 blocks per row
  unsigned int num_unaligned; // blocks that don't align with 28
  
  // statistics ===========
  long long read_num;
  long long write_num;
  
  long long hit_num;
  long long miss_num;
  long long hit_read;
  long long miss_read;
  long long miss_compulsory;
  long long miss_conflict;
  
  long long current_dirty_num;
  long long evicted_dirty_num;
  long long peak_dirty_num;
  
  std::map<unsigned int, unsigned int> conflictmissdist; // <row#, miss#>
  //=======================
  
  std::vector<cache_block_t> * cache_sets;
  
  Alloy_Cache(unsigned int sizeinMB);
  ~Alloy_Cache()
  {
    free(cache_sets);
  }
  void cache_access(int iaddr, int isrd, int coreid);
};

Alloy_Cache::Alloy_Cache(unsigned int sizeinMB)
{ 
  this->num_sets = sizeinMB<<(20-6);
  this->num_rows = this->num_sets / 28;
  this->num_unaligned = this->num_sets % 28;
  if (this->num_unaligned > 0) this->num_rows++;
    
  cache_sets = new std::vector<cache_block_t>(this->num_sets, cache_block_t());
  this->read_num = 0;
  this->write_num = 0;
  this->hit_num = 0;
  this->miss_num = 0;
  this->hit_read = 0;
  this->miss_read = 0;
  this->miss_conflict = 0;
  this->miss_compulsory = 0;
  this->current_dirty_num = 0;
  this->evicted_dirty_num = 0;
  this->peak_dirty_num = 0;
}
void Alloy_Cache::cache_access(int iaddr, int isrd, int coreid)
{
  bool isRead; 
  unsigned int address;
  unsigned int sets, rows, tags;
  std::vector<cache_block_t> * cache_ptr = this->cache_sets;
  
  address = (unsigned int)iaddr;
  isRead = (isrd == 0)? true : false; // in trace file 0-read 1-write
  
  if (isRead) 
    this->read_num++;
  else
    this->write_num++;
  
  // address mapping
  tags = address>>6;
  sets = tags % this->num_sets;
  rows = sets/28;
  
  // Check cache hit/miss
  if ( ((*cache_ptr)[sets].tag == tags) && ((*cache_ptr)[sets].isValid == true) )
  // hit
  {
    // 
    this->hit_num++;
    if (isRead) 
      this->hit_read++;
    else if ((*cache_ptr)[sets].isDirty == false)
    {
      (*cache_ptr)[sets].isDirty = true;
      this->current_dirty_num++;     
    }
  }
  else // miss
  {
    // statistics =====================
    this->miss_num++;
    if (isRead) this->miss_read++;
    if ((*cache_ptr)[sets].isValid == false)
    {
      this->miss_compulsory++;
    }
    else
    {
      this->miss_conflict++;
      this->conflictmissdist[(rows/DIST_BIN_SIZE)*DIST_BIN_SIZE]++;
    }
    
    if ((*cache_ptr)[sets].isDirty)
    {
      this->current_dirty_num--;
      this->evicted_dirty_num++;
    }
    
    if (isRead == false)
    {
      this->current_dirty_num++;
    }
    
    if (this->peak_dirty_num < this->current_dirty_num)
    {
      this->peak_dirty_num = this->current_dirty_num;
    }
    
    //=================================
    
    
    (*cache_ptr)[sets].tag = tags;
    (*cache_ptr)[sets].isValid = true;
    (*cache_ptr)[sets].isDirty = (isRead)? false : true;
    
  }
}
//==========================================================================
//==========================================================================
// Victim Cache
class victim_cache
{
public:
  unsigned int entry_num;
  
  std::list<cache_block_t> cache_list;
  
  void victim_insert(cache_block_t * target);
  std::list<cache_block_t>::iterator isHitvictim(unsigned int addr);
  
  victim_cache(unsigned int size) 
  {
    entry_num = size;
  }
};

void victim_cache::victim_insert(cache_block_t * target)
{
  cache_block_t newblock;
  
  newblock = *target;

  if (cache_list.size() >= entry_num)
  {
    cache_list.pop_front();
  }
  
  cache_list.push_back(newblock);
}

std::list<cache_block_t>::iterator victim_cache::isHitvictim(unsigned int addr)
{
  std::list<cache_block_t>::iterator iter;
  for (iter=cache_list.begin();iter!=cache_list.end();iter++)
  {
    if (iter->addr == addr)
    {
      return iter;
    }
  }
  
  return iter;
}

//==========================================================================
//==========================================================================
// Reuse distance record

class virtual_set_record
{
public:
  unsigned int set_size; // how many blocks in one virtual set
  unsigned int set_num; // # of sets
  unsigned int block_num;
  
  std::vector<long long> access_cnt;
  std::map<unsigned int, long long> reuse_dist; // <distance, hit#>
  
  
  virtual_set_record(unsigned int cachesizeinMB, unsigned int virtualset_size)
  {
    set_size = virtualset_size;
    
    block_num = cachesizeinMB<<(20-6);
    set_num = block_num/virtualset_size;
    if (0 != (block_num%virtualset_size)) set_num++;
    
    access_cnt.clear();
    access_cnt.resize(set_num, 0);
    
    reuse_dist.clear();
  }
  
  void access_update(unsigned int addr)
  {
    unsigned int blocks, sets;
    
    blocks = (addr>>6)%block_num;
    sets = blocks/set_size;
    
    access_cnt[sets]++;
  }
  
  long long get_cnt(unsigned int addr)
  {
    unsigned int blocks, sets;
    
    blocks = (addr>>6)%block_num;
    sets = blocks/set_size;
    
    return access_cnt[sets];
  }
  
  void reuse_dist_update(unsigned int distance)
  {
    reuse_dist[(distance/REUSE_DIST_UNIT)*REUSE_DIST_UNIT]++;
  }
  
  void reuse_dist_dump(ofstream & output_file)
  {
    std::map<unsigned int, long long>::iterator iter;
    long long total_num = 0;
    long long peak_num = 0;
    unsigned int peak_position = 0;
    unsigned int tmp_sum = 0;
    long long position_mean = 0;
    unsigned int position_90 = 0;
    unsigned int position_80 = 0;
    unsigned int position_70 = 0;
    unsigned int position_001 = 0;
    unsigned int position_005 = 0;
    unsigned int position_010 = 0;
    
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {
	total_num+=iter->second;
	if (iter->second > peak_num)
	{
	  peak_num = iter->second;
	  peak_position = iter->first;
	}
    }
    
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {
	position_mean += iter->first * iter->second;
    }
    
    position_mean /= total_num;
    
    tmp_sum = 0;
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {
	tmp_sum+=iter->second;
	if ( tmp_sum >= (total_num*0.9) )
	{
	  position_90 = iter->first;
	  break;
	}
    }
    
    tmp_sum = 0;
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {
	tmp_sum+=iter->second;
	if ( tmp_sum >= (total_num*0.8) )
	{
	  position_80 = iter->first;
	  break;
	}
    }
    
    tmp_sum = 0;
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {
	tmp_sum+=iter->second;
	if ( tmp_sum >= (total_num*0.7) )
	{
	  position_70 = iter->first;
	  break;
	}
    }
    
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {

	if ( iter->second <= peak_num*0.01 )
	{
	  position_001 = iter->first;
	  break;
	}
    }
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {

	if ( iter->second <= peak_num*0.05 )
	{
	  position_005 = iter->first;
	  break;
	}
    }
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {

	if ( iter->second <= peak_num*0.1 )
	{
	  position_010 = iter->first;
	  break;
	}
    }
	
    
    output_file<<"Total Elements: "<<total_num<<endl;
    output_file<<"Peak Position: "<<endl;
    output_file<<"    Value: "<<peak_num<<endl;
    output_file<<"    Position: "<<peak_position<<endl<<endl;
    output_file<<"Mean Position: "<<position_mean<<endl;
    output_file<<"90% Total Position: "<<position_90<<endl;
    output_file<<"80% Total Position: "<<position_80<<endl;
    output_file<<"70% Total Position: "<<position_70<<endl;
    output_file<<"1% Peak Position: "<<position_001<<endl;
    output_file<<"5% Peak Position: "<<position_005<<endl;
    output_file<<"10% Peak Position: "<<position_010<<endl;
    output_file<<"------------------------"<<endl;
    for (iter=reuse_dist.begin();iter!=reuse_dist.end();iter++)
    {
	output_file<<"["<<iter->first<<"-"<<iter->first+REUSE_DIST_UNIT-1<<"]  "<<iter->second<<endl;
    }
  }
};

//==========================================================================
//==========================================================================
// Regular Set-associative Cache
class regular_set_t
{
public:
  unsigned int num_ways;
  
  std::list<cache_block_t *> * cache_ways;
};

class Regular_Cache
{
public:
  unsigned int num_sets;
  unsigned int num_ways;
  
  std::vector<regular_set_t> * cache_sets;
  // statistics ===========
  long long read_num;
  long long write_num;
  long long zero_reuse_num;

  long long hit_num;
  long long miss_num;
  long long hit_read;
  long long miss_read;
  long long miss_compulsory;
  long long miss_conflict;
  
  long long current_dirty_num;
  long long evicted_dirty_num;
  long long peak_dirty_num;
  
  long long doa_eviction;
  
  std::map<unsigned int, long long> access_dist; // <cache block #, access #>
  std::map<unsigned int, long long> doa_dist; //<cache block #, doa block #>
  std::map<unsigned int, long long> shorthit_dist; //<cache block #, block #>
  
  Regular_Cache(unsigned int sizeinMB, unsigned int way_num);
  void cache_access(int iaddr, int isrd, int coreid, int usetime);
  
  void free_cache(void);
  
  ~Regular_Cache()
  {
    //free(cache_sets);
  }
};
Regular_Cache::Regular_Cache(unsigned int sizeinMB, unsigned int way_num)
{
  this->num_sets = (sizeinMB<<(20-6))/way_num;
  if ( ((sizeinMB<<(20-6))%way_num)!=0)  this->num_sets++;
  this->num_ways = way_num;
    
  cache_sets = new std::vector<regular_set_t>(this->num_sets, regular_set_t());
  
  // init regular_set_t
  std::vector<regular_set_t>::iterator iter;
  for (iter=cache_sets->begin();iter!=cache_sets->end();iter++)
  {
    iter->num_ways = way_num;
    iter->cache_ways = new std::list<cache_block_t *>;
    for (int i=0;i<way_num;i++)
    {
      cache_block_t * newblock = new cache_block_t();
      iter->cache_ways->push_back(newblock);
    }
  }
  

  this->read_num = 0;
  this->write_num = 0;
  this->zero_reuse_num = 0;

  this->hit_num = 0;
  this->miss_num = 0;
  this->hit_read = 0;
  this->miss_read = 0;
  this->miss_conflict = 0;
  this->miss_compulsory = 0;
  
  this->current_dirty_num = 0;
  this->evicted_dirty_num = 0;
  this->peak_dirty_num = 0;
  
  this->doa_eviction = 0;
}

void Regular_Cache::free_cache(void)
{
  std::list<cache_block_t *> * set_ptr;
  std::vector<regular_set_t>::iterator iter;
  std::list<cache_block_t *>::iterator iter2;
  
  for (iter=cache_sets->begin();iter!=cache_sets->end();iter++)
  {
    set_ptr = iter->cache_ways;
    for (iter2=set_ptr->begin();iter2!=set_ptr->end();iter2++)
    {
      free(*iter2);
    }
    free(set_ptr);
  }
}

void Regular_Cache::cache_access(int iaddr, int isrd, int coreid, int usetime)
{
  bool isRead; 
  unsigned int address;
  unsigned int sets, tags;
  std::list<cache_block_t *> * cache_set_ptr;
    
  address = (unsigned int)iaddr;
  isRead = (isrd == 0)? true : false; // in trace file 0-read 1-write
  
  if(usetime>1) 
  {    
      if (isRead) 
        this->read_num++;
      else
        this->write_num++;
      
      // address mapping
      tags = address>>6;
      //sets = tags % this->num_sets;
      if(CACHE_ASSOC==32)
        sets = (tags>>5) % this->num_sets;
      else
        sets = tags % this->num_sets;


      // access distribution
      access_dist[(sets/ACCESS_DIST_UNIT)*ACCESS_DIST_UNIT]++;
      
      cache_set_ptr = (*cache_sets)[sets].cache_ways;
      
      // Check cache hit/miss
      std::list<cache_block_t *>::iterator iter;
      for (iter=cache_set_ptr->begin();iter!=cache_set_ptr->end();iter++)
      {
        if ( ((*iter)->tag == tags) && ((*iter)->isValid == true) )
        {
          break;
        }
      }
  
  
  
      if ( iter != cache_set_ptr->end() )
      // hit
      {
        // 
        this->hit_num++;
        if (isRead) 
          this->hit_read++;
        else
        {
          (*iter)->isDirty = true;
          this->current_dirty_num++;
        }
        cache_block_t * hit_block = *iter;
        
        hit_block->hit_cnt++;
        if (virtualsets) 
        {
          long long current_cnt;
          current_cnt = virtualsets->get_cnt(address); //get current set access count
          
          // get reuse info
          virtualsets->reuse_dist_update(current_cnt - hit_block->last_hit_virtualset_cnt);
          
          if ( (current_cnt - hit_block->last_hit_virtualset_cnt) < 45 )
    	shorthit_dist[(sets/SHORTHIT_DIST_UNIT)*SHORTHIT_DIST_UNIT]++;
          
          // update block counter
          hit_block->last_hit_virtualset_cnt = current_cnt;
          
        }
        
        cache_set_ptr->erase(iter);
        cache_set_ptr->push_back(hit_block);
      }
      else // miss
      {
        // find out victim
        cache_block_t * victim =  cache_set_ptr->front();

        
        // statistics =====================
        this->miss_num++;
        if (isRead) this->miss_read++;
        
        if (victim->isValid == false)
        {
          this->miss_compulsory++;
        }
        else
        {
          this->miss_conflict++;
          
          // get evicted block info
          if (victim->hit_cnt == 0)
          {
    	doa_eviction++;
    	doa_dist[(sets/DOA_DIST_UNIT)*DOA_DIST_UNIT]++;
          }      
        }
        
        if (isRead == false)
        {
          this->current_dirty_num++;
        }
        
        if (victim->isDirty)
        {
          this->current_dirty_num--;
          this->evicted_dirty_num++;
        }
               
        if (this->peak_dirty_num < this->current_dirty_num)
        {
          this->peak_dirty_num = this->current_dirty_num;
        }
        
        //=================================
        
        
        victim->tag = tags;
        victim->addr = address;
        victim->isValid = true;
        victim->isDirty = (isRead)? false : true;
        
        victim->hit_cnt = 0;
        if (virtualsets) 
          victim->last_hit_virtualset_cnt = virtualsets->get_cnt(address);
        
        cache_set_ptr->pop_front(); // evict LRU

    // other replacement policy
    /*
        std::list<cache_block_t *>::iterator insertplace = cache_set_ptr->begin();
        int place = std::rand() % 31 +1;
        std::advance (insertplace,place);
        cache_set_ptr->insert(insertplace,victim);
    */
    //-----------
        cache_set_ptr->push_back(victim);
     }
  }
  else
      this->zero_reuse_num++;
}

//==========================================================================

void print_usage(void)
{
  cout<<"Usage: ./<EXE> --cache-size <in MB(<4096)> --cache-assoc <(<100)> --core-num <# of cores> --trace-file <trace file path & name> --output-file <output file path & name>"<<endl;
}

void parse_input(int argc, char ** argv)
{
  // init 
  CACHE_SIZE = 0;
  CACHE_ASSOC = 0;
  CORE_NUM = 0;
  SCHEME = 0;
  TRACE_FILE = "";
  
  cout<<"Reading Input Arguments..."<<endl;
  
  for (int i=1; i<argc; i++)
  {
    std::string iarg;
    
    iarg = argv[i];
    
    if ( iarg == "--cache-size" )
    {
      i++;
      CACHE_SIZE = atoi(argv[i]);
    }
    if ( iarg == "--cache-assoc" )
    {
      i++;
      CACHE_ASSOC = atoi(argv[i]);
    }
    if ( iarg == "--core-num" )
    {
      i++;
      CORE_NUM = atoi(argv[i]);
    }
    if ( iarg == "--virtual-size" )
    {
      i++;
      VIRTUAL_SIZE = atoi(argv[i]);
    }
    if ( iarg == "--scheme" )
    {
      i++;
      SCHEME = atoi(argv[i]);
    }
    if ( iarg == "--trace-file" )
    {
      i++;
      TRACE_FILE = argv[i];
    }
    if ( iarg == "--output-file" )
    {
      i++;
      OUTPUT_FILE = argv[i];
    }
  }
  
  
  
  // use default value if unset
  if ( (CACHE_SIZE == 0)||(CACHE_SIZE>4096) )
  {
    CACHE_SIZE = 128;
    cout<<"  Cache Size: using default value - "<<CACHE_SIZE<<" MB"<<endl;
  }
  if (CACHE_ASSOC == 0)
  {
    CACHE_ASSOC = 1;
    cout<<"  Cache Assoc: using default value - "<<CACHE_ASSOC<<" way"<<endl;
  }
  if (CORE_NUM == 0)
  {
    CORE_NUM = 8;
    cout<<"  Core Number: using default value - "<<CORE_NUM<<" core"<<endl;
  }
  if (VIRTUAL_SIZE == 0)
  {
    VIRTUAL_SIZE = 28;
    cout<<"  Virtual Size: using default value - "<<VIRTUAL_SIZE<<" blocks"<<endl;
  }
  if (TRACE_FILE == "")
  {
    cout<<"Trace File: ERROR"<<endl;
    assert(0);
  }
  if (OUTPUT_FILE == "")
  {
    OUTPUT_FILE = "sim.out";
  }
  
  cout<<"  Cache Size  : "<<CACHE_SIZE<<" MB"<<endl;
  cout<<"  Cache Assoc : "<<CACHE_ASSOC<<" way"<<endl;
  cout<<"  Core Number : "<<CORE_NUM<<" core"<<endl;
  cout<<"  Virtual Size: "<<VIRTUAL_SIZE<<" blocks"<<endl;
  if(SCHEME == 0) 
      cout<<"  Scheme      : regular"<<endl;
  else
      cout<<"  Scheme      : bypass zero reuse"<<endl;
  
  cout<<"  Trace File  : "<<TRACE_FILE<<endl;
  cout<<"  Output File : "<<OUTPUT_FILE<<endl;
}


bool sanity_check(int isrd, int coreid, int addr)
{
  if ((isrd<0)||(isrd>1))
    return false;
  
  if ((coreid<0)||(coreid>=CORE_NUM))
    return false;
  
  //if (addr<0)
  //{
  //  return false;
  //}
  
  return true;
}


void cache_init(void)
{
//   if (0)//(CACHE_ASSOC == 1)
//   {
//     alloycache = new Alloy_Cache(CACHE_SIZE);
//     cout<<"Cache Init"<<endl;
//     cout<<"  sets#: "<<alloycache->num_sets<<" row#: "<<alloycache->num_rows<<" unaligned#: "<<alloycache->num_unaligned<<endl;
//   }
//   else
//   {
    dramcache = new Regular_Cache(CACHE_SIZE, CACHE_ASSOC);
    cout<<"Cache Init"<<endl;
    cout<<"  sets#: "<<dramcache->num_sets<<" way#: "<<dramcache->num_ways<<endl;
    virtualsets = new virtual_set_record(CACHE_SIZE, VIRTUAL_SIZE);
//   }
}

void dump_output(void)
{
  ofstream output_file;
//   if (alloycache)
//   {
//     output_file.open(OUTPUT_FILE.c_str());
//     output_file<<"Cache Size: "<<CACHE_SIZE<<" MB";
//     output_file<<"Cache Assoc: "<<CACHE_ASSOC<<" way"<<endl;
//     output_file<<"=================="<<endl;
//     
//     output_file<<"Write#: "<<alloycache->write_num<<" Read#: "<<alloycache->read_num<<endl;
//     output_file<<"Hit#: "<<alloycache->hit_num<<" Read Hit#: "<<alloycache->hit_read<<endl;
//     output_file<<"Miss#: "<<alloycache->miss_num<<" Read Miss#: "<<alloycache->miss_read<<endl;
//     output_file<<"Compulsory Miss#: "<<alloycache->miss_compulsory<<" Conflict Miss#: "<<alloycache->miss_conflict<<endl;
//     output_file<<"Peak Dirty#: "<<alloycache->peak_dirty_num<<" Total Evicted Dirty#: "<<alloycache->evicted_dirty_num<<endl;
// 
//     output_file<<"=================="<<endl;
//     std::map<unsigned int, unsigned int>::iterator it;
//     
//     for (it=alloycache->conflictmissdist.begin(); it!=alloycache->conflictmissdist.end();it++)
//     {
//       output_file<<"["<<it->first<<"-"<<it->first+DIST_BIN_SIZE-1<<"]  "<<it->second<<endl;
//     }
//     
//     output_file.close();
//   }
  if (dramcache)
  {
    output_file.open(OUTPUT_FILE.c_str());
    output_file<<"Cache Size: "<<CACHE_SIZE<<" MB"<<endl;
    output_file<<"Cache Assoc: "<<dramcache->num_ways<<" way"<<endl;
    output_file<<"Cache Sets: "<<dramcache->num_sets<<" sets"<<endl;
    output_file<<"=============================="<<endl;
    output_file<<"Access#: "<<dramcache->write_num + dramcache->read_num<<endl;
    output_file<<"zero reuse access#: "<<dramcache->zero_reuse_num<<endl;
    output_file<<"    Write#: "<<dramcache->write_num<<endl;
    output_file<<"    Read# : "<<dramcache->read_num<<endl<<endl;
    output_file<<"Hit#: "<<dramcache->hit_num<<endl;
    output_file<<"    Hit rate# : "<<(float) dramcache->hit_num/(dramcache->write_num + dramcache->read_num)<<endl;
    output_file<<"    Read Hit# : "<<dramcache->hit_read<<endl;
    output_file<<"    Write Hit#: "<<dramcache->hit_num - dramcache->hit_read<<endl<<endl;
    output_file<<"Miss#: "<<dramcache->miss_num<<endl;
    output_file<<"    Read Miss# : "<<dramcache->miss_read<<endl;
    output_file<<"    Write Miss#: "<<dramcache->miss_num - dramcache->miss_read<<endl<<endl;
    output_file<<"    Compulsory Miss#: "<<dramcache->miss_compulsory<<endl;
    output_file<<"    Conflict Miss#  : "<<dramcache->miss_conflict<<endl;
    output_file<<"    DoA Eviction#   : "<<dramcache->doa_eviction<<endl<<endl;
    output_file<<"         Peak Dirty#: "<<dramcache->peak_dirty_num<<endl;
    output_file<<"Total Evicted Dirty#: "<<dramcache->evicted_dirty_num<<endl;
    
    output_file<<"=============================="<<endl;
    if (virtualsets)
    {
      
      virtualsets->reuse_dist_dump(output_file);
      output_file<<"=============================="<<endl;
    }    
    
    // access distribution
    output_file<<"Access Distribution: "<<endl;
    std::map<unsigned int, long long>::iterator iter;
    for (iter=dramcache->access_dist.begin(); iter!=dramcache->access_dist.end();iter++)
    {
      output_file<<"["<<iter->first<<"-"<<iter->first+ACCESS_DIST_UNIT-1<<"]  "<<iter->second<<endl;
    }   
    
    output_file<<"=============================="<<endl;
    output_file<<"ShortHit Distribution: "<<endl;
    
    for (iter=dramcache->shorthit_dist.begin(); iter!=dramcache->shorthit_dist.end();iter++)
    {
      output_file<<"["<<iter->first<<"-"<<iter->first+SHORTHIT_DIST_UNIT-1<<"]  "<<iter->second<<endl;
    } 
    
    
    output_file<<"=============================="<<endl;
    output_file<<"DoA Distribution: "<<endl;
    
    for (iter=dramcache->doa_dist.begin(); iter!=dramcache->doa_dist.end();iter++)
    {
      output_file<<"["<<iter->first<<"-"<<iter->first+DOA_DIST_UNIT-1<<"]  "<<iter->second<<endl;
    } 
    
    
    output_file<<"=============================="<<endl;
    output_file.close();
    
    dramcache->free_cache();
  }
  
}



int main (int argc, char ** argv)
{
  ifstream trace_file;
  ofstream trace_file2;

  int isrd, coreid, addr, pc, usetime;
  int counter = 0;
  std::vector<int> access;
  std::map<int, std::vector<int> > usage;
  usage.clear();
 
  // get input arguments
  parse_input(argc,argv);
  
  // open trace file
  //TRACE_FILE.append("_usage"); //include reuse info

  trace_file.open(TRACE_FILE.c_str());
  if (! trace_file.is_open())
  {
    cout<<"Trace File: fail to open"<<endl;
    assert(0);
  }
  
  // init data structure
  cache_init();
  
  counter = 0;
  while(1)
  {
    // read one line from trace file
    trace_file>>isrd>>coreid>>addr>>pc;
    if (!trace_file.good())
    {
      break;
    }
    access.push_back(0);

/*
    // verify the sanity of data
    if (!sanity_check(isrd, coreid, addr))
    {
      cout<<"ERROR Trace File Data"<<endl;
      assert(0);
    }
    //cout<<"isrd: "<<isrd<<" coreid: "<<coreid<<" addr: "<<addr<<" usetime: "<<usetime<<endl;


    if (virtualsets)
      virtualsets->access_update((unsigned int)addr);
    
    if(SCHEME == 0) //regular
    {
        if (dramcache)
            dramcache->cache_access(addr, isrd, coreid, 2);
    }
    else //bypass zero reuse
    {
        if (dramcache)
            dramcache->cache_access(addr, isrd, coreid, usetime);
    }
*/

    
        
    
//==========address profiling=========        
    if(usage.find(addr) == usage.end()) {
        usage[addr].push_back(1);
        usage[addr].push_back(counter);
    }
    else{
        usage[addr].front()++;
        access[usage[addr].back()] = counter - usage[addr].back();
        usage[addr].back() = counter;

    }
    counter++;
//==========end address profiling=====



    //     else if (alloycache)
//       alloycache->cache_access(addr, isrd, coreid);
    
  }
  
  // close trace file
  trace_file.close();

  // open trace file
  
  
//=========profiling write file==================  
  
  trace_file.open(TRACE_FILE.c_str());
  if (! trace_file.is_open())
  {
    cout<<"Trace File: fail to open"<<endl;
    assert(0);
  }
  //TRACE_FILE.append("_usage");
  trace_file2.open(OUTPUT_FILE.c_str());
  counter = 0;
  while(1) 
  {
    trace_file>>isrd>>coreid>>addr>>pc;
    if (!trace_file.good())
    {
      break;
    }

    trace_file2<<isrd<<setw(3)<<coreid<<setw(10)<<addr<<setw(12)<<pc<<setw(6)<<usage[addr].front()<<setw(10)<<access[counter]<<endl;
    //usage[addr]--;
    counter++;

  }
  trace_file.close();
  trace_file2.close();

//=========end profiling======================




  // dump output data
  //dump_output();
  
  
  return 0;
}
