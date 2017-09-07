/******************************************************* 
 *
 * This structure is Last Writer Module for bundle commitment
 *
 ***************************************************** */

#ifndef	__CPU_O3_LWModule_HH__
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
		unsigned num_of_entries;
		unsigned num_of_arch_regs;

	private:
		struct LWIL
		{
			LWIL()
			{}
			LWIL(bool _flag, unsigned _rel_index)
			:flag(_flag), rel_index(_rel_index)
			{}
			/* Is arch deset register of last writer */
			bool flag;
			/* Relative Index from start instruction in a bundle */
			unsigned rel_index;
		};

		/* History Information from a bundle */
		struct BundleHistory 
		{
			BundleHistory()
			{
				start_inst_pc = 0;
				end_inst_pc = 0;
				size = 0;
				exception = false;
				count = 0;
				bundle_commit = false;
				valid = false;
			}
			/* The PC address of start instruction in a bundle */
			Addr start_inst_pc;
			/* The PC address of end instruction in a bundle */
			Addr end_inst_pc;
			/* Number of instructions in a bundle */
			unsigned size;
			/* Has exception for recovery */
			bool exception;
			/* Last Writer Index List */
			std::vector<LWIL> lw_index_list;
			/* Committed instruction count */
			unsigned count;
			/* Bundle commit enable flag */
			bool bundle_commit;
			/* Valid bit*/
			bool valid;

			void operator=(BundleHistory &_bundleHistory)
			{
				this->start_inst_pc = _bundleHistory.start_inst_pc;
				this->end_inst_pc = _bundleHistory.end_inst_pc;
				this->size = _bundleHistory.size;
				this->count = _bundleHistory.count;
				this->bundle_commit = _bundleHistory.bundle_commit;
				this->lw_index_list.swap(_bundleHistory.lw_index_list);
				this->valid = _bundleHistory.valid;
			}
		};

	/* Constructor */
	public:
		unsigned bundle_buffer_limit;

		LWModule(const std::string &_my_name,
				unsigned _num_of_entries,
				unsigned _num_of_arch_regs,
				unsigned _bundle_buffer_limit);

	/* Destructor */
		~LWModule() 
		{
		}

		/* History Table */
		std::deque<BundleHistory> historyTable;

		/* Bundle Buffer */
		std::deque<BundleHistory> bundleBuffer;

	/* Member Functions */
		std::string getName() const
		{
			return name;
		}

		void makeNewLWIL(std::vector<LWIL> &lwil);

		void initializeHistoryTable(BundleHistory &history_table);

		void writeHistoryTable(BundleHistory &history_table, unsigned index);

		/* Clear specific index entry of history table */
		void clearBundleHistory(unsigned _index);

		/* Write relative index in a bundle to LWIL */
		void writeLWToLWIL(unsigned _history_table_index, 
				RegIndex dest_arch_reg, unsigned _bundle_rel_index);

		void clearLWIL(std::vector<LWIL> &lwil);
		void incrementSize(unsigned _history_table_index);
		void setException(unsigned _history_table_index);
		void resetException(unsigned _history_table_index);
		void incrementCount(unsigned _history_table_index);
		void setStartInstAddr(unsigned _history_table_index, Addr _start_addr);
		void setEndInstAddr(unsigned _history_table_index, Addr _end_addr);
		void setLWFlag(unsigned _history_table_index, unsigned _reg_idx);
		void resetLWFlag(unsigned _history_table_index, unsigned _reg_idx);
		void setLWRelIdx(unsigned _history_table_index, unsigned reg_idx, unsigned _rel_idx);
		void setBundleCommit(unsigned _history_table_index);
		void resetBundleCommit(unsigned _history_table_index);
		void setCountZero(unsigned _history_table_index);
		void setValid(unsigned _history_table_index);
		void resetValid(unsigned _history_table_index);
};

#endif
