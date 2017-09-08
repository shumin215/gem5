#ifndef __CPU_O3_LWModule_HH__
#define __CPU_O3_LWModule_HH__

#include <iostream>
#include <utility>
#include <algorithm>
#include <vector>
#include <deque>

#include "base/trace.hh"
#include "config/the_isa.hh"
#include "cpu/o3/comm.hh"
#include "debug/LWModule.hh"
#include "base/types.hh"

class LWModule
{
	public:
		typedef TheISA::RegIndex RegIndex;

		const std::string name;
		unsigned num_of_ht_entries;
		unsigned num_of_arch_regs;
		unsigned bundle_queue_limit;

		struct LWIT_Entry
		{
			LWIT_Entry()
			{
				flag = false;
				rel_idx = 0;
			}

			bool flag;

			unsigned rel_idx;
		};

		struct BundleHistoryEntry
		{
			BundleHistoryEntry()
			{
				valid = false;
				size = 0;
				start_pc = 0;
				end_pc = 0;
			}

			bool valid;

			unsigned size;

			// Start seq number of instruction
			Addr start_pc;

			// End seq number of instruction 
			Addr end_pc;

			// Last Writer Index Table
			std::vector<LWIT_Entry> lwit;
		};

		struct BundleQueueEntry
		{
			BundleQueueEntry()
			{
				exception = false;
				size = 0;
				count = 0;
				bundle_commit = false;
				start_pc = 0;
				start_seq = 0;
				end_seq = 0;
			}

			// exception flag
			bool exception;

			// Size of bundle instructions
			unsigned size;

			// count of committed instruction
			unsigned count;

			// If the bundle is able to commit
			bool bundle_commit;

			// Start instructio pc
			Addr start_pc;

			uint64_t start_seq;

			uint64_t end_seq;
			
			// Last Writer Index Table
			std::vector<LWIT_Entry> lwit;
		};

		// Constructor
		LWModule(const std::string &_my_name,
		unsigned _num_of_ht_entries,
		unsigned _num_of_arch_regs,
		unsigned _bundle_buffer_limit);
		
		// Destructor 
		~LWModule()
		{}

		// Bundle History Table
		std::vector<BundleHistoryEntry> bundleHistoryTable;

		// Bundle Queue 
		std::deque<BundleQueueEntry> bundleQueue;

		// Member Functions

		/********************************************************
		// For Bundle History Table 
		// ***************************************************/
		void setValidToBHT(unsigned idx)
		{
			this->bundleHistoryTable[idx].valid = true;
		}

		void clearValidToBHT(unsigned idx)
		{
			this->bundleHistoryTable[idx].valid = false;
		}

		void setStartPCToBHT(unsigned idx, Addr _start_pc)
		{
			this->bundleHistoryTable[idx].start_pc = _start_pc;
		}

		void setEndPCToBHT(unsigned idx, Addr _end_pc)
		{
			this->bundleHistoryTable[idx].end_pc = _end_pc;
		}

		unsigned incrementSizeToBHT(unsigned idx)
		{
			int result = this->bundleHistoryTable[idx].size++;

			return result;
		}

		void clearBHTEntry(unsigned idx);

		void setLWFlagToBHT(unsigned idx, RegIndex dest_reg_idx);

		void setLWRelIdx(unsigned idx, RegIndex dest_reg_idx, unsigned _rel_idx);

		/********************************************************
		// For Bundle Queue 
		// ***************************************************/

		void setStartSeqToBQ(BundleQueueEntry &bq_entry, uint64_t seq_num)
		{
			bq_entry.start_seq = seq_num;
		}

		void setEndSeqToBQ(BundleQueueEntry &bq_entry, uint64_t seq_num)
		{
			bq_entry.end_seq = seq_num;
		}

		void setStartPCToBQ(BundleQueueEntry &bq_entry, Addr _start_pc)
		{
			bq_entry.start_pc = _start_pc;
		}

		void setBundleCommitToBQ(BundleQueueEntry &bq_entry)
		{
			bq_entry.bundle_commit = true;
		}

		void setExceptionToBQ(BundleQueueEntry &bq_entry)
		{
			bq_entry.exception = true;
		}

		void setSizeToBQ(BundleQueueEntry &bq_entry, unsigned _size)
		{
			bq_entry.size = _size;
		}

		void incrementCountToBQ(BundleQueueEntry &bq_entry)
		{
			bq_entry.count++;
		}

		void setLWITTOBQ(BundleQueueEntry &bq_entry, BundleHistoryEntry &bundle_history);

	private:
};

#endif
