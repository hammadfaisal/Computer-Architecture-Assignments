#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>
#include <iomanip>

using namespace std;

struct cache_block {
    bool valid;
    bool dirty;
    long long tag;
    long long lru;
};

static const int L1_access_time = 1;
static const int L2_access_time = 20;
static const int memory_access_time = 200;

long long hexToDec(std::string hex) {
    long long dec;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> dec;
    return dec;
}

struct cache {
    long long size;
    long long associativity;
    long long block_size;
    long long num_blocks;
    long long num_sets;
    long long num_ways;
    long long num_reads;
    long long num_writes;
    long long num_read_misses;
    long long num_write_misses;
    long long num_writebacks;
    vector<vector<cache_block> > blocks;
};

long long log2(long long n) {
    long long ans = 0;
    while (n > 1) {
        n /= 2;
        ans++;
    }
    return ans;
}

void init_cache(cache &c, long long size, long long associativity, long long block_size) {
    c.size = size;
    c.associativity = associativity;
    c.block_size = block_size;
    c.num_blocks = size / block_size;
    c.num_sets = c.num_blocks / associativity;
    c.num_ways = associativity;
    c.num_reads = 0;
    c.num_writes = 0;
    c.num_read_misses = 0;
    c.num_write_misses = 0;
    c.num_writebacks = 0;
    c.blocks.resize(c.num_sets);
    for (long long i = 0; i < c.num_sets; i++) {
        c.blocks[i].resize(c.num_ways);
        for (long long j = 0; j < c.num_ways; j++) {
            c.blocks[i][j].valid = false;
            c.blocks[i][j].dirty = false;
            c.blocks[i][j].tag = 0;
            c.blocks[i][j].lru = 0;
        }
    }
}

void update_lru(cache &c, long long set, long long way) {
    for (long long i = 0; i < c.num_ways; i++) {
        if (i == way) {
            c.blocks[set][i].lru = 0;
        } else {
            c.blocks[set][i].lru++;
        }
    }
}

long long find_replace_block(cache &c, long long index) {
    long long replace_block =0;
    for (long long i = 0; i < c.num_ways; i++) {
        if (!c.blocks[index][i].valid) {
            return i;
            break;
        }
        if (c.blocks[index][i].lru > c.blocks[index][replace_block].lru) {
            replace_block = i;
        }
    }
    return replace_block;
}

//two level cache read. L1 is inclusive of L2 and they have different sizes and associativities but same block size
void read_cache(cache &L1, cache &L2, long long address) {
    //cout<<std::hex<< "read address: " << address << endl;
    long long offset_bits = log2(L1.block_size);
    long long index_bits_l1 = log2(L1.num_sets);
    long long index_bits_l2 = log2(L2.num_sets);
    long long tag_bits_l1 = 62 - offset_bits - index_bits_l1;
    long long tag_bits_l2 = 62 - offset_bits - index_bits_l2;

    long long offset_mask = (1LL << offset_bits) - 1;
    long long index_mask_l1 = ((1LL << index_bits_l1) - 1) << offset_bits;
    long long index_mask_l2 = ((1LL << index_bits_l2) - 1) << offset_bits;
    long long tag_mask_l1 = ((1LL << tag_bits_l1) - 1) << (offset_bits + index_bits_l1);
    long long tag_mask_l2 = ((1LL << tag_bits_l2) - 1) << (offset_bits + index_bits_l2);

    long long offset = address & offset_mask;
    long long index_l1 = (address & index_mask_l1) >> offset_bits;
    long long index_l2 = (address & index_mask_l2) >> offset_bits;
    long long tag_l1 = (address & tag_mask_l1) >> (offset_bits + index_bits_l1);
    long long tag_l2 = (address & tag_mask_l2) >> (offset_bits + index_bits_l2);

    bool l1_hit = false;
    bool l2_hit = false;

    // search L1 cache
    L1.num_reads++;
    for (long long i = 0; i < L1.num_ways; i++) {
        if (L1.blocks[index_l1][i].valid && L1.blocks[index_l1][i].tag == tag_l1) {
            // hit in L1 cache
            l1_hit = true;
            update_lru(L1, index_l1, i);
            break;
        }
    }

    if (!l1_hit) {
        // L1 miss, check L2 cache
        L1.num_read_misses++;
        long long replace_block_l1 = find_replace_block(L1, index_l1);
        // if L1 block is dirty and valid, write it back to L2
        if (L1.blocks[index_l1][replace_block_l1].valid && L1.blocks[index_l1][replace_block_l1].dirty) {
            L1.num_writebacks++;
            L2.num_writes++;
            // inclusive so evicted block exists in L2. so just calculate appropriate index, tag and make it dirty
            long long address_l2 = (L1.blocks[index_l1][replace_block_l1].tag << (offset_bits + index_bits_l1)) | (index_l1 << offset_bits);
            long long index_l2_evict = (address_l2 & index_mask_l2) >> offset_bits;
            long long tag_l2_evict = (address_l2 & tag_mask_l2) >> (offset_bits + index_bits_l2);
            bool done = false;//check if found
            for (long long j = 0; j < L2.num_ways; j++) {
                if (L2.blocks[index_l2_evict][j].tag == tag_l2_evict) {
                    assert(L2.blocks[index_l2_evict][j].valid);
                    L2.blocks[index_l2_evict][j].dirty = true;
                    update_lru(L2, index_l2_evict, j);
                    done = true;
                    break;
                }
            }
            //cout<<std::hex<< tag_l1 << " " << index_l1 << " " << replace_block_l1 << endl;
            //cout<<std::hex<< "writeback address: " << address_l2 << endl;
            assert(done);
        }
        
        L2.num_reads++;
        // search L2 cache
        for (long long i = 0; i < L2.num_ways; i++) {
            if (L2.blocks[index_l2][i].valid && L2.blocks[index_l2][i].tag == tag_l2) {
                // hit in L2 cache
                l2_hit = true;
                update_lru(L2, index_l2, i);
                break;
            }
        }
        
        if (!l2_hit) {
            // L2 miss, fetch block from memory
            L2.num_read_misses++;
            // find LRU block or invalid block in L2 cache and replace it
            long long replace_block_l2 = find_replace_block(L2, index_l2);

            if (L2.blocks[index_l2][replace_block_l2].valid) {
                // invalidate corresponding L1 block if it exists
                long long evict_address = (L2.blocks[index_l2][replace_block_l2].tag << (offset_bits + index_bits_l2)) | (index_l2 << offset_bits);
                long long evict_index_l1 = (evict_address & index_mask_l1) >> offset_bits;
                long long evict_tag_l1 = (evict_address & tag_mask_l1) >> (offset_bits + index_bits_l1);
                bool dirty_invalid = false;
                for (long long i = 0; i < L1.num_ways; i++) {
                    if (L1.blocks[evict_index_l1][i].valid && L1.blocks[evict_index_l1][i].tag == evict_tag_l1) {
                        L1.blocks[evict_index_l1][i].valid = false;
                        if (L1.blocks[evict_index_l1][i].dirty) {
                            dirty_invalid = true;
                        }
                        break;
                    }
                }
                if (dirty_invalid) {
                    L1.num_writebacks++;
                    L2.blocks[index_l2][replace_block_l2].dirty = true;
                }
                
            }


            // if L2 block is dirty and valid, write it back to memory
            if (L2.blocks[index_l2][replace_block_l2].valid && L2.blocks[index_l2][replace_block_l2].dirty) {
                L2.num_writebacks++;
            }
            
            // read block from memory
            L2.blocks[index_l2][replace_block_l2].valid = true;
            L2.blocks[index_l2][replace_block_l2].dirty = false;
            L2.blocks[index_l2][replace_block_l2].tag = tag_l2;
            update_lru(L2, index_l2, replace_block_l2);
        }

        // replace L1 block with L2 block
        L1.blocks[index_l1][replace_block_l1].valid = true;
        L1.blocks[index_l1][replace_block_l1].dirty = false;
        L1.blocks[index_l1][replace_block_l1].tag = tag_l1;
        update_lru(L1, index_l1, replace_block_l1);
    }
}

void write_cache(cache &L1, cache &L2, long long address) {
    //cout<<std::hex<< "write address: " << address << endl;
    long long offset_bits = log2(L1.block_size);
    long long index_bits_l1 = log2(L1.num_sets);
    long long index_bits_l2 = log2(L2.num_sets);
    long long tag_bits_l1 = 62 - offset_bits - index_bits_l1;
    long long tag_bits_l2 = 62 - offset_bits - index_bits_l2;

    long long offset_mask = (1LL << offset_bits) - 1;
    long long index_mask_l1 = ((1LL << index_bits_l1) - 1) << offset_bits;
    long long index_mask_l2 = ((1LL << index_bits_l2) - 1) << offset_bits;
    long long tag_mask_l1 = ((1LL << tag_bits_l1) - 1) << (offset_bits + index_bits_l1);
    long long tag_mask_l2 = ((1LL << tag_bits_l2) - 1) << (offset_bits + index_bits_l2);

    long long offset = address & offset_mask;
    long long index_l1 = (address & index_mask_l1) >> offset_bits;
    long long index_l2 = (address & index_mask_l2) >> offset_bits;
    long long tag_l1 = (address & tag_mask_l1) >> (offset_bits + index_bits_l1);
    long long tag_l2 = (address & tag_mask_l2) >> (offset_bits + index_bits_l2);

    bool l1_hit = false;
    bool l2_hit = false;

    // search L1 cache
    L1.num_writes++;
    for (long long i = 0; i < L1.num_ways; i++) {
        if (L1.blocks[index_l1][i].valid && L1.blocks[index_l1][i].tag == tag_l1) {
            // hit in L1 cache
            l1_hit = true;
            L1.blocks[index_l1][i].dirty = true;
            update_lru(L1, index_l1, i);
            break;
        }
    }

    if (!l1_hit) {
        // L1 miss, check L2 cache
        L1.num_write_misses++;
        long long replace_block_l1 = find_replace_block(L1, index_l1);
        // if L1 block is dirty and valid, write it back to L2
        if (L1.blocks[index_l1][replace_block_l1].valid && L1.blocks[index_l1][replace_block_l1].dirty) {
            L1.num_writebacks++;
            L2.num_writes++;
            // inclusive so evicted block exists in L2. so just calculate appropriate index, tag and make it dirty
            long long address_l2 = (L1.blocks[index_l1][replace_block_l1].tag << (offset_bits + index_bits_l1)) | (index_l1 << offset_bits);
            long long index_l2_evict = (address_l2 & index_mask_l2) >> offset_bits;
            long long tag_l2_evict = (address_l2 & tag_mask_l2) >> (offset_bits + index_bits_l2);
            bool done = false;//check if found
            for (long long j = 0; j < L2.num_ways; j++) {
                if (L2.blocks[index_l2_evict][j].tag == tag_l2_evict) {
                    assert(L2.blocks[index_l2_evict][j].valid);
                    L2.blocks[index_l2_evict][j].dirty = true;
                    update_lru(L2, index_l2_evict, j);
                    done = true;
                    break;
                }
            }
            assert(done);
        }
        
        L2.num_reads++;
        // search L2 cache
        for (long long i = 0; i < L2.num_ways; i++) {
            if (L2.blocks[index_l2][i].valid && L2.blocks[index_l2][i].tag == tag_l2) {
                // hit in L2 cache
                l2_hit = true;
                update_lru(L2, index_l2, i);
                break;
            }
        }
        
        if (!l2_hit) {
            // L2 miss, fetch block from memory
            L2.num_read_misses++;
            // find LRU block or invalid block in L2 cache and replace it
            long long replace_block_l2 = find_replace_block(L2, index_l2);

            if (L2.blocks[index_l2][replace_block_l2].valid) {
                // invalidate corresponding L1 block if it exists
                long long evict_address = (L2.blocks[index_l2][replace_block_l2].tag << (offset_bits + index_bits_l2)) | (index_l2 << offset_bits);
                long long evict_index_l1 = (evict_address & index_mask_l1) >> offset_bits;
                long long evict_tag_l1 = (evict_address & tag_mask_l1) >> (offset_bits + index_bits_l1);
                bool dirty_invalid = false;
                for (long long i = 0; i < L1.num_ways; i++) {
                    if (L1.blocks[evict_index_l1][i].valid && L1.blocks[evict_index_l1][i].tag == evict_tag_l1) {
                        L1.blocks[evict_index_l1][i].valid = false;
                        if (L1.blocks[evict_index_l1][i].dirty) {
                            dirty_invalid = true;
                        }
                        break;
                    }
                }
                if (dirty_invalid) {
                    L1.num_writebacks++;
                    L2.blocks[index_l2][replace_block_l2].dirty = true;
                }
            }

            // if L2 block is dirty and valid, write it back to memory
            if (L2.blocks[index_l2][replace_block_l2].valid && L2.blocks[index_l2][replace_block_l2].dirty) {
                L2.num_writebacks++;
            }
            

            // read block from memory
            L2.blocks[index_l2][replace_block_l2].valid = true;
            L2.blocks[index_l2][replace_block_l2].dirty = false;
            L2.blocks[index_l2][replace_block_l2].tag = tag_l2;
            update_lru(L2, index_l2, replace_block_l2);
        }
        // read block from L2 cache and update L1 cache
        L1.blocks[index_l1][replace_block_l1].valid = true;
        L1.blocks[index_l1][replace_block_l1].dirty = true;
        L1.blocks[index_l1][replace_block_l1].tag = tag_l1;
        update_lru(L1, index_l1, replace_block_l1);
        // cout <<std::hex<<tag_l1<<" "<<index_l1<<" "<<replace_block_l1<<endl;
    }
}

int main(int argc, char ** argv) {
    // argc = 7;
    // argv[1] = "1";
    // argv[2] = "2";
    // argv[3] = "2";
    // argv[4] = "2";
    // argv[5] = "2";
    // argv[6] = "./obj/t.txt";
    if (argc != 7)
	{
		std::cerr << "Required 6 arguments";
		return 1;
	}
    std::ifstream file(argv[6]);
    if (!file.is_open()) {
        std::cerr << "File not found";
        return 1;
    }
    if (__builtin_popcount(atoi(argv[2])) != 1 || __builtin_popcount(atoi(argv[4])) != 1) {
        std::cerr << "Cache size must be a power of 2";
        return 1;
    }
    if (__builtin_popcount(atoi(argv[3])) != 1 || __builtin_popcount(atoi(argv[5])) != 1) {
        std::cerr << "Cache associativity must be a power of 2";
        return 1;
    }
    if (__builtin_popcount(atoi(argv[1])) != 1) {
        std::cerr << "Cache block size must be a power of 2";
        return 1;
    }
    cache L1, L2;
    init_cache(L1, stoll(argv[2]), stoll(argv[3]), stoll(argv[1]));
    init_cache(L2, stoll(argv[4]), stoll(argv[5]), stoll(argv[1]));
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string access_type;
        std::string address;
        if (!(iss >> access_type >> address)) {
            break;
        }
        if (access_type == "r") {
            read_cache(L1, L2, hexToDec(address));
        } else if (access_type == "w") {
            write_cache(L1, L2, hexToDec(address));
        } else {
            std::cerr << "Invalid access type";
            return 1;
        }
    }
    file.close();
    std::cout << "====== Simulation results ======" << std::endl;
    std::cout << "a. number of L1 reads:\t\t\t\t" << L1.num_reads << std::endl;
    std::cout << "b. number of L1 read misses:\t\t\t" << L1.num_read_misses << std::endl;
    std::cout << "c. number of L1 writes:\t\t\t\t" << L1.num_writes << std::endl;
    std::cout << "d. number of L1 write misses:\t\t\t" << L1.num_write_misses << std::endl;
    std::cout << "e. L1 miss rate:\t\t\t\t\t" << std::fixed << std::setprecision(4) << ((double)L1.num_read_misses+L1.num_write_misses) / (L1.num_reads + L1.num_writes) << std::endl;
    std::cout << "f. number of writebacks from L1 memory:\t\t" << L1.num_writebacks << std::endl;
    std::cout << "g. number of L2 reads:\t\t\t\t" << L2.num_reads << std::endl;
    std::cout << "h. number of L2 read misses:\t\t\t" << L2.num_read_misses << std::endl;
    std::cout << "i. number of L2 writes:\t\t\t\t" << L2.num_writes << std::endl;
    std::cout << "j. number of L2 write misses:\t\t\t" << L2.num_write_misses << std::endl;
    std::cout << "k. L2 miss rate:\t\t\t\t\t" << std::fixed << std::setprecision(4) << ((double)L2.num_read_misses+L2.num_write_misses) / (L2.num_reads + L2.num_writes) << std::endl;
    std::cout << "l. number of writebacks from L2 memory:\t\t" << L2.num_writebacks << std::endl;
    long long total_time = (L1.num_reads+L1.num_writes)*L1_access_time+(L2.num_reads+L2.num_writes)*L2_access_time+(L2.num_writebacks+L2.num_read_misses)*memory_access_time;
    std::cout << "m. total time spent in nanoseconds:\t\t\t" << total_time << std::endl;
}


