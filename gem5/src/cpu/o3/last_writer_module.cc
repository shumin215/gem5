/******************************************************* 
 *
 * This structure is Last Writer List (LWL) for bundle commitment
 *
 ***************************************************** */

#include "cpu/o3/last_writer_module.hh"
#include "config/the_isa.hh"
#include "debug/LWModule.hh"

LWModule::LWModule(const std::string &_my_name,
				unsigned _num_of_entries,
				unsigned _num_of_arch_regs,
				unsigned _bundle_buffer_limit)
	: name(_my_name), num_of_entries(_num_of_entries), num_of_arch_regs(_num_of_arch_regs),
	bundle_buffer_limit(_bundle_buffer_limit)
{
//	DPRINTF(LWModule, "LWModule: LWModule is launched\n");
	/* Initialize */
	historyTable.resize(num_of_entries);

	for(int i=0; i<num_of_entries; i++)
	{
		initializeHistoryTable(historyTable[i]);
	}

	bundleBuffer.resize(bundle_buffer_limit);
}

void LWModule::makeNewLWIL(std::vector<LWIL> &lwil)
{
	lwil.resize(num_of_arch_regs);

	for(int i=0; i<num_of_arch_regs; i++)
	{
		lwil.push_back(LWIL(false, 0));
	}
}

void LWModule::initializeHistoryTable(BundleHistory &history_table)
{
	history_table.start_inst_pc = 0;
	history_table.end_inst_pc = 0;
	history_table.size = 0;
	history_table.exception = false;
	history_table.count = 0;
	history_table.bundle_commit = false;

	makeNewLWIL(history_table.lw_index_list);

//	history_table.lw_index_list.resize(num_of_arch_regs);
//
//	for(int i=0; i<num_of_arch_regs; i++)
//	{
//		history_table.lw_index_list.push_back(LWIL(false, 0));
//	}
}

void LWModule::clearBundleHistory(unsigned _index)
{
//	BundleHistory bundle_history = this->historyTable[_index];

	historyTable[_index].start_inst_pc = 0;
	historyTable[_index].end_inst_pc = 0;
	historyTable[_index].size = 0;
	historyTable[_index].exception = false;
	historyTable[_index].count = 0;
	historyTable[_index].bundle_commit = false;

	for(int i=0; i<num_of_arch_regs; i++)
	{
		historyTable[_index].lw_index_list[i].flag = false;
		historyTable[_index].lw_index_list[i].rel_index = 0;
	}
}

void LWModule::writeLWToLWIL(unsigned _history_table_index, 
		RegIndex dest_arch_reg, unsigned _bundle_rel_index)
{
	historyTable[_history_table_index].lw_index_list[dest_arch_reg].rel_index = _bundle_rel_index;

	historyTable[_history_table_index].size++;
}

void LWModule::clearLWIL(std::vector<LWIL> &lwil)
{
	for(int i=0; i<num_of_arch_regs; i++)
	{
		lwil[i].flag = false;
		lwil[i].rel_index = 0;
	}
}

void LWModule::incrementSize(unsigned _history_table_index)
{
	historyTable[_history_table_index].size++;
}

void LWModule::setException(unsigned _history_table_index)
{
	historyTable[_history_table_index].exception = true;
}

void LWModule::resetException(unsigned _history_table_index)
{
	historyTable[_history_table_index].exception = false;
}

void LWModule::incrementCount(unsigned _history_table_index)
{
	historyTable[_history_table_index].count++;
}

void LWModule::setStartInstAddr(unsigned _history_table_index, Addr _start_addr)
{
	historyTable[_history_table_index].start_inst_pc = _start_addr;
}

void LWModule::setEndInstAddr(unsigned _history_table_index, Addr _end_addr)
{
	historyTable[_history_table_index].end_inst_pc = _end_addr;
}

void LWModule::setLWFlag(unsigned _history_table_index, unsigned _reg_idx)
{
	historyTable[_history_table_index].lw_index_list[_reg_idx].flag = true;
}

void LWModule::resetLWFlag(unsigned _history_table_index, unsigned _reg_idx)
{
	historyTable[_history_table_index].lw_index_list[_reg_idx].flag = false;
}

void LWModule::setLWRelIdx(unsigned _history_table_index, unsigned reg_idx, unsigned _rel_idx)
{
	historyTable[_history_table_index].lw_index_list[reg_idx].rel_index = _rel_idx;
}

void LWModule::setBundleCommit(unsigned _history_table_index)
{
	historyTable[_history_table_index].bundle_commit = true;
}

void LWModule::resetBundleCommit(unsigned _history_table_index)
{
	historyTable[_history_table_index].bundle_commit = false;
}

void LWModule::setCountZero(unsigned _history_table_index)
{
	historyTable[_history_table_index].count = 0;
}
