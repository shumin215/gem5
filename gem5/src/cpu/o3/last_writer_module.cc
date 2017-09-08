
#include "cpu/o3/last_writer_module.hh"
#include "config/the_isa.hh"
#include "debug/LWModule.hh"

LWModule::LWModule(const std::string &_my_name,
		unsigned _num_of_ht_entries,
		unsigned _num_of_arch_regs,
		unsigned _bundle_queue_limit)
	:name(_my_name), num_of_ht_entries(_num_of_ht_entries), 
	num_of_arch_regs(_num_of_arch_regs), bundle_queue_limit(_bundle_queue_limit)
{
	bundleHistoryTable.resize(num_of_ht_entries);

	for(int i=0; i<num_of_ht_entries; i++)
	{
		bundleHistoryTable[i].lwit.resize(num_of_arch_regs);
	}

	bundleQueue.resize(bundle_queue_limit);
}

void LWModule::clearBHTEntry(unsigned idx)
{
	bundleHistoryTable[idx].valid = 0;
	bundleHistoryTable[idx].size = 0;
	bundleHistoryTable[idx].start_pc = 0;
	bundleHistoryTable[idx].end_pc = 0;

	for(int i=0; i<num_of_arch_regs; i++)
	{
		bundleHistoryTable[idx].lwit[i].flag = false;
		bundleHistoryTable[idx].lwit[i].rel_idx = 0;
	}
}

void LWModule::setLWFlagToBHT(unsigned idx, RegIndex dest_reg_idx)
{
	bundleHistoryTable[idx].lwit[dest_reg_idx].flag = true;
}

void LWModule::setLWRelIdx(unsigned idx, RegIndex dest_reg_idx, unsigned _rel_idx)
{
	bundleHistoryTable[idx].lwit[dest_reg_idx].rel_idx = _rel_idx;
}

void LWModule::setLWITTOBQ(BundleQueueEntry &bq_entry, BundleHistoryEntry &bundle_history)
{
	bq_entry.lwit.resize(num_of_arch_regs);

	for(int i=0; i<num_of_arch_regs; i++)
	{
		bq_entry.lwit[i].flag = bundle_history.lwit[i].flag;
		bq_entry.lwit[i].rel_idx = bundle_history.lwit[i].rel_idx;
	}
}
