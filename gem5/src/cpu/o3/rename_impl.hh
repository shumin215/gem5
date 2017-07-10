/*
 * Copyright (c) 2010-2012, 2014-2015 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2004-2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Kevin Lim
 *          Korey Sewell
 */

#ifndef __CPU_O3_RENAME_IMPL_HH__
#define __CPU_O3_RENAME_IMPL_HH__

#include <list>

#include "arch/isa_traits.hh"
#include "arch/registers.hh"
#include "config/the_isa.hh"
#include "cpu/o3/rename.hh"
#include "cpu/reg_class.hh"
#include "debug/Activity.hh"
#include "debug/Rename.hh"
#include "debug/O3PipeView.hh"
#include "params/DerivO3CPU.hh"

#define BUFFER_SIZE 100

using namespace std;

template <class Impl>
DefaultRename<Impl>::DefaultRename(O3CPU *_cpu, DerivO3CPUParams *params)
    : cpu(_cpu),
      iewToRenameDelay(params->iewToRenameDelay),
      decodeToRenameDelay(params->decodeToRenameDelay),
      commitToRenameDelay(params->commitToRenameDelay),
      renameWidth(params->renameWidth),
	  isMovEliUsed(params->isMovEliUsed),
      commitWidth(params->commitWidth),
      numThreads(params->numThreads),
      maxPhysicalRegs(params->numPhysIntRegs + params->numPhysFloatRegs
                      + params->numPhysCCRegs)
{
    if (renameWidth > Impl::MaxWidth)
        fatal("renameWidth (%d) is larger than compiled limit (%d),\n"
             "\tincrease MaxWidth in src/cpu/o3/impl.hh\n",
             renameWidth, static_cast<int>(Impl::MaxWidth));

    // @todo: Make into a parameter.
    skidBufferMax = (decodeToRenameDelay + 1) * params->decodeWidth;
}

template <class Impl>
std::string
DefaultRename<Impl>::name() const
{
    return cpu->name() + ".rename";
}

template <class Impl>
void
DefaultRename<Impl>::regStats()
{
    renameSquashCycles
        .name(name() + ".SquashCycles")
        .desc("Number of cycles rename is squashing")
        .prereq(renameSquashCycles);
    renameIdleCycles
        .name(name() + ".IdleCycles")
        .desc("Number of cycles rename is idle")
        .prereq(renameIdleCycles);
    renameBlockCycles
        .name(name() + ".BlockCycles")
        .desc("Number of cycles rename is blocking")
        .prereq(renameBlockCycles);
    renameSerializeStallCycles
        .name(name() + ".serializeStallCycles")
        .desc("count of cycles rename stalled for serializing inst")
        .flags(Stats::total);
    renameRunCycles
        .name(name() + ".RunCycles")
        .desc("Number of cycles rename is running")
        .prereq(renameIdleCycles);
    renameUnblockCycles
        .name(name() + ".UnblockCycles")
        .desc("Number of cycles rename is unblocking")
        .prereq(renameUnblockCycles);
    renameRenamedInsts
        .name(name() + ".RenamedInsts")
        .desc("Number of instructions processed by rename")
        .prereq(renameRenamedInsts);
    renameSquashedInsts
        .name(name() + ".SquashedInsts")
        .desc("Number of squashed instructions processed by rename")
        .prereq(renameSquashedInsts);
    renameROBFullEvents
        .name(name() + ".ROBFullEvents")
        .desc("Number of times rename has blocked due to ROB full")
        .prereq(renameROBFullEvents);
    renameIQFullEvents
        .name(name() + ".IQFullEvents")
        .desc("Number of times rename has blocked due to IQ full")
        .prereq(renameIQFullEvents);
    renameLQFullEvents
        .name(name() + ".LQFullEvents")
        .desc("Number of times rename has blocked due to LQ full")
        .prereq(renameLQFullEvents);
    renameSQFullEvents
        .name(name() + ".SQFullEvents")
        .desc("Number of times rename has blocked due to SQ full")
        .prereq(renameSQFullEvents);
    renameFullRegistersEvents
        .name(name() + ".FullRegisterEvents")
        .desc("Number of times there has been no free registers")
        .prereq(renameFullRegistersEvents);
    renameRenamedOperands
        .name(name() + ".RenamedOperands")
        .desc("Number of destination operands rename has renamed")
        .prereq(renameRenamedOperands);
    renameRenameLookups
        .name(name() + ".RenameLookups")
        .desc("Number of register rename lookups that rename has made")
        .prereq(renameRenameLookups);
    renameCommittedMaps
        .name(name() + ".CommittedMaps")
        .desc("Number of HB maps that are committed")
        .prereq(renameCommittedMaps);
    renameUndoneMaps
        .name(name() + ".UndoneMaps")
        .desc("Number of HB maps that are undone due to squashing")
        .prereq(renameUndoneMaps);
    renamedSerializing
        .name(name() + ".serializingInsts")
        .desc("count of serializing insts renamed")
        .flags(Stats::total)
        ;
    renamedTempSerializing
        .name(name() + ".tempSerializingInsts")
        .desc("count of temporary serializing insts renamed")
        .flags(Stats::total)
        ;
    renameSkidInsts
        .name(name() + ".skidInsts")
        .desc("count of insts added to the skid buffer")
        .flags(Stats::total)
        ;
    intRenameLookups
        .name(name() + ".int_rename_lookups")
        .desc("Number of integer rename lookups")
        .prereq(intRenameLookups);
    fpRenameLookups
        .name(name() + ".fp_rename_lookups")
        .desc("Number of floating rename lookups")
        .prereq(fpRenameLookups);

	/****************** MOV Elimination *********************/
    numOfMOVInst
        .name(name() + ".numOfMOVInst")
        .desc("Number of MOV instructions");

    numOfMOVInstTwoOperands
        .name(name() + ".numOfMOVInstTwoOperands")
        .desc("Number of MOV instructions having only two operands");

    numOfEliminatedInst
        .name(name() + ".numOfEliminatedInst")
        .desc("Number of Eliminated MOV instructions");

    numOfNotImmediateMov
        .name(name() + ".numOfNotImmediateMov")
        .desc("Number of MOV instructions that don't have immediate value");

    numOfImmediateMov
        .name(name() + ".numOfImmediateMov")
        .desc("Number of MOV instructions that have immediate value");

    numOfMovHavingPC
        .name(name() + ".numOfMovHavingPC")
        .desc("Number of MOV instructions that have source reg as PC register (r15)");

    numOfMOVEQ
        .name(name() + ".numOfMOVEQ")
        .desc("Number of MOVEQ instructions");

    numOfMOVNE
        .name(name() + ".numOfMOVNE")
        .desc("Number of MOVNE instructions");
}

template <class Impl>
void
DefaultRename<Impl>::regProbePoints()
{
    ppRename = new ProbePointArg<DynInstPtr>(cpu->getProbeManager(), "Rename");
    ppSquashInRename = new ProbePointArg<SeqNumRegPair>(cpu->getProbeManager(),
                                                        "SquashInRename");
}

template <class Impl>
void
DefaultRename<Impl>::setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr)
{
    timeBuffer = tb_ptr;

    // Setup wire to read information from time buffer, from IEW stage.
    fromIEW = timeBuffer->getWire(-iewToRenameDelay);

    // Setup wire to read infromation from time buffer, from commit stage.
    fromCommit = timeBuffer->getWire(-commitToRenameDelay);

    // Setup wire to write information to previous stages.
    toDecode = timeBuffer->getWire(0);
}

template <class Impl>
void
DefaultRename<Impl>::setRenameQueue(TimeBuffer<RenameStruct> *rq_ptr)
{
    renameQueue = rq_ptr;

    // Setup wire to write information to future stages.
    toIEW = renameQueue->getWire(0);
}

template <class Impl>
void
DefaultRename<Impl>::setDecodeQueue(TimeBuffer<DecodeStruct> *dq_ptr)
{
    decodeQueue = dq_ptr;

    // Setup wire to get information from decode.
    fromDecode = decodeQueue->getWire(-decodeToRenameDelay);
}

template <class Impl>
void
DefaultRename<Impl>::startupStage()
{
    resetStage();
}

template <class Impl>
void
DefaultRename<Impl>::resetStage()
{
    _status = Inactive;

    resumeSerialize = false;
    resumeUnblocking = false;

    // Grab the number of free entries directly from the stages.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        renameStatus[tid] = Idle;

        freeEntries[tid].iqEntries = iew_ptr->instQueue.numFreeEntries(tid);
        freeEntries[tid].lqEntries = iew_ptr->ldstQueue.numFreeLoadEntries(tid);
        freeEntries[tid].sqEntries = iew_ptr->ldstQueue.numFreeStoreEntries(tid);
        freeEntries[tid].robEntries = commit_ptr->numROBFreeEntries(tid);
        emptyROB[tid] = true;

        stalls[tid].iew = false;
        serializeInst[tid] = NULL;

        instsInProgress[tid] = 0;
        loadsInProgress[tid] = 0;
        storesInProgress[tid] = 0;

        serializeOnNextInst[tid] = false;
    }
}

template<class Impl>
void
DefaultRename<Impl>::setActiveThreads(list<ThreadID> *at_ptr)
{
    activeThreads = at_ptr;
}


template <class Impl>
void
DefaultRename<Impl>::setRenameMap(RenameMap rm_ptr[])
{
    for (ThreadID tid = 0; tid < numThreads; tid++)
        renameMap[tid] = &rm_ptr[tid];
}

/* Mov Elimination: set real rename map */
template <typename Impl>
void DefaultRename<Impl>::setCommitRenameMap(RenameMap rename_map_ptr[])
{
    for (ThreadID tid = 0; tid < numThreads; tid++)
        commitRenameMap[tid] = &rename_map_ptr[tid];
}

template <class Impl>
void
DefaultRename<Impl>::setFreeList(FreeList *fl_ptr)
{
    freeList = fl_ptr;
}

template<class Impl>
void
DefaultRename<Impl>::setScoreboard(Scoreboard *_scoreboard)
{
    scoreboard = _scoreboard;
}

template <class Impl>
bool
DefaultRename<Impl>::isDrained() const
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (instsInProgress[tid] != 0 ||
            !historyBuffer[tid].empty() ||
            !skidBuffer[tid].empty() ||
            !insts[tid].empty() ||
            (renameStatus[tid] != Idle && renameStatus[tid] != Running))
            return false;
    }
    return true;
}

template <class Impl>
void
DefaultRename<Impl>::takeOverFrom()
{
    resetStage();
}

template <class Impl>
void
DefaultRename<Impl>::drainSanityCheck() const
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        assert(historyBuffer[tid].empty());
        assert(insts[tid].empty());
        assert(skidBuffer[tid].empty());
        assert(instsInProgress[tid] == 0);
    }
}

template <class Impl>
void
DefaultRename<Impl>::squash(const InstSeqNum &squash_seq_num, ThreadID tid)
{
    DPRINTF(Rename, "[tid:%u]: Squashing instructions.\n",tid);

    // Clear the stall signal if rename was blocked or unblocking before.
    // If it still needs to block, the blocking should happen the next
    // cycle and there should be space to hold everything due to the squash.
    if (renameStatus[tid] == Blocked ||
        renameStatus[tid] == Unblocking) {
        toDecode->renameUnblock[tid] = 1;

        resumeSerialize = false;
        serializeInst[tid] = NULL;
    } else if (renameStatus[tid] == SerializeStall) {
        if (serializeInst[tid]->seqNum <= squash_seq_num) {
            DPRINTF(Rename, "Rename will resume serializing after squash\n");
            resumeSerialize = true;
            assert(serializeInst[tid]);
        } else {
            resumeSerialize = false;
            toDecode->renameUnblock[tid] = 1;

            serializeInst[tid] = NULL;
        }
    }

    // Set the status to Squashing.
    renameStatus[tid] = Squashing;

    // Squash any instructions from decode.
    for (int i=0; i<fromDecode->size; i++) {
        if (fromDecode->insts[i]->threadNumber == tid &&
            fromDecode->insts[i]->seqNum > squash_seq_num) {
            fromDecode->insts[i]->setSquashed();
            wroteToTimeBuffer = true;
        }

    }

    // Clear the instruction list and skid buffer in case they have any
    // insts in them.
    insts[tid].clear();

    // Clear the skid buffer in case it has any data in it.
    skidBuffer[tid].clear();

    doSquash(squash_seq_num, tid);
}

template <class Impl>
void
DefaultRename<Impl>::tick()
{
    wroteToTimeBuffer = false;

    blockThisCycle = false;

    bool status_change = false;

    toIEWIndex = 0;

    sortInsts();

    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    // Check stall and squash signals.
    while (threads != end) {
        ThreadID tid = *threads++;

        DPRINTF(Rename, "Processing [tid:%i]\n", tid);

        status_change = checkSignalsAndUpdate(tid) || status_change;

        rename(status_change, tid);
    }

    if (status_change) {
        updateStatus();
    }

    if (wroteToTimeBuffer) {
        DPRINTF(Activity, "Activity this cycle.\n");
        cpu->activityThisCycle();
    }

    threads = activeThreads->begin();

    while (threads != end) {
        ThreadID tid = *threads++;

        // If we committed this cycle then doneSeqNum will be > 0
        if (fromCommit->commitInfo[tid].doneSeqNum != 0 &&
            !fromCommit->commitInfo[tid].squash &&
            renameStatus[tid] != Squashing) {

            removeFromHistory(fromCommit->commitInfo[tid].doneSeqNum,
                                  tid);
        }
    }

    // @todo: make into updateProgress function
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        instsInProgress[tid] -= fromIEW->iewInfo[tid].dispatched;
        loadsInProgress[tid] -= fromIEW->iewInfo[tid].dispatchedToLQ;
        storesInProgress[tid] -= fromIEW->iewInfo[tid].dispatchedToSQ;
        assert(loadsInProgress[tid] >= 0);
        assert(storesInProgress[tid] >= 0);
        assert(instsInProgress[tid] >=0);
    }

}

template<class Impl>
void
DefaultRename<Impl>::rename(bool &status_change, ThreadID tid)
{
    // If status is Running or idle,
    //     call renameInsts()
    // If status is Unblocking,
    //     buffer any instructions coming from decode
    //     continue trying to empty skid buffer
    //     check if stall conditions have passed

    if (renameStatus[tid] == Blocked) {
        ++renameBlockCycles;
    } else if (renameStatus[tid] == Squashing) {
        ++renameSquashCycles;
    } else if (renameStatus[tid] == SerializeStall) {
        ++renameSerializeStallCycles;
        // If we are currently in SerializeStall and resumeSerialize
        // was set, then that means that we are resuming serializing
        // this cycle.  Tell the previous stages to block.
        if (resumeSerialize) {
            resumeSerialize = false;
            block(tid);
            toDecode->renameUnblock[tid] = false;
        }
    } else if (renameStatus[tid] == Unblocking) {
        if (resumeUnblocking) {
            block(tid);
            resumeUnblocking = false;
            toDecode->renameUnblock[tid] = false;
        }
    }

    if (renameStatus[tid] == Running ||
        renameStatus[tid] == Idle) {
        DPRINTF(Rename, "[tid:%u]: Not blocked, so attempting to run "
                "stage.\n", tid);

        renameInsts(tid);
    } else if (renameStatus[tid] == Unblocking) {
        renameInsts(tid);

        if (validInsts()) {
            // Add the current inputs to the skid buffer so they can be
            // reprocessed when this stage unblocks.
            skidInsert(tid);
        }

        // If we switched over to blocking, then there's a potential for
        // an overall status change.
        status_change = unblock(tid) || status_change || blockThisCycle;
    }
}

template <class Impl>
void
DefaultRename<Impl>::renameInsts(ThreadID tid)
{
    // Instructions can be either in the skid buffer or the queue of
    // instructions coming from decode, depending on the status.
    int insts_available = renameStatus[tid] == Unblocking ?
        skidBuffer[tid].size() : insts[tid].size();

    // Check the decode queue to see if instructions are available.
    // If there are no available instructions to rename, then do nothing.
    if (insts_available == 0) {
        DPRINTF(Rename, "[tid:%u]: Nothing to do, breaking out early.\n",
                tid);
        // Should I change status to idle?
        ++renameIdleCycles;
        return;
    } else if (renameStatus[tid] == Unblocking) {
        ++renameUnblockCycles;
    } else if (renameStatus[tid] == Running) {
        ++renameRunCycles;
    }

    DynInstPtr inst;

    // Will have to do a different calculation for the number of free
    // entries.
    int free_rob_entries = calcFreeROBEntries(tid);
    int free_iq_entries  = calcFreeIQEntries(tid);
    int min_free_entries = free_rob_entries;

    FullSource source = ROB;

    if (free_iq_entries < min_free_entries) {
        min_free_entries = free_iq_entries;
        source = IQ;
    }

    // Check if there's any space left.
    if (min_free_entries <= 0) {
        DPRINTF(Rename, "[tid:%u]: Blocking due to no free ROB/IQ/ "
                "entries.\n"
                "ROB has %i free entries.\n"
                "IQ has %i free entries.\n",
                tid,
                free_rob_entries,
                free_iq_entries);

        blockThisCycle = true;

        block(tid);

        incrFullStat(source);

        return;
    } else if (min_free_entries < insts_available) {
        DPRINTF(Rename, "[tid:%u]: Will have to block this cycle."
                "%i insts available, but only %i insts can be "
                "renamed due to ROB/IQ/LSQ limits.\n",
                tid, insts_available, min_free_entries);

        insts_available = min_free_entries;

        blockThisCycle = true;

        incrFullStat(source);
    }

    InstQueue &insts_to_rename = renameStatus[tid] == Unblocking ?
        skidBuffer[tid] : insts[tid];

    DPRINTF(Rename, "[tid:%u]: %i available instructions to "
            "send iew.\n", tid, insts_available);

    DPRINTF(Rename, "[tid:%u]: %i insts pipelining from Rename | %i insts "
            "dispatched to IQ last cycle.\n",
            tid, instsInProgress[tid], fromIEW->iewInfo[tid].dispatched);

    // Handle serializing the next instruction if necessary.
    if (serializeOnNextInst[tid]) {
        if (emptyROB[tid] && instsInProgress[tid] == 0) {
            // ROB already empty; no need to serialize.
            serializeOnNextInst[tid] = false;
        } else if (!insts_to_rename.empty()) {
            insts_to_rename.front()->setSerializeBefore();
        }
    }

    int renamed_insts = 0;

    while (insts_available > 0 &&  toIEWIndex < renameWidth) {
        DPRINTF(Rename, "[tid:%u]: Sending instructions to IEW.\n", tid);

        assert(!insts_to_rename.empty());

        inst = insts_to_rename.front();

        //For all kind of instructions, check ROB and IQ first
        //For load instruction, check LQ size and take into account the inflight loads
        //For store instruction, check SQ size and take into account the inflight stores

        if (inst->isLoad()) {
            if (calcFreeLQEntries(tid) <= 0) {
                DPRINTF(Rename, "[tid:%u]: Cannot rename due to no free LQ\n");
                source = LQ;
                incrFullStat(source);
                break;
            }
        }

        if (inst->isStore()) {
            if (calcFreeSQEntries(tid) <= 0) {
                DPRINTF(Rename, "[tid:%u]: Cannot rename due to no free SQ\n");
                source = SQ;
                incrFullStat(source);
                break;
            }
        }

        insts_to_rename.pop_front();

        if (renameStatus[tid] == Unblocking) {
            DPRINTF(Rename,"[tid:%u]: Removing [sn:%lli] PC:%s from rename "
                    "skidBuffer\n", tid, inst->seqNum, inst->pcState());
        }

        if (inst->isSquashed()) {
            DPRINTF(Rename, "[tid:%u]: instruction %i with PC %s is "
                    "squashed, skipping.\n", tid, inst->seqNum,
                    inst->pcState());

            ++renameSquashedInsts;

            // Decrement how many instructions are available.
            --insts_available;

            continue;
        }

        DPRINTF(Rename, "[tid:%u]: Processing instruction [sn:%lli] with "
                "PC %s.\n", tid, inst->seqNum, inst->pcState());

        // Check here to make sure there are enough destination registers
        // to rename to.  Otherwise block.
        if (!renameMap[tid]->canRename(inst->numIntDestRegs(),
                                       inst->numFPDestRegs(),
                                       inst->numCCDestRegs())) {
            DPRINTF(Rename, "Blocking due to lack of free "
                    "physical registers to rename to.\n");
            blockThisCycle = true;
            insts_to_rename.push_front(inst);
            ++renameFullRegistersEvents;

            break;
        }

        // Handle serializeAfter/serializeBefore instructions.
        // serializeAfter marks the next instruction as serializeBefore.
        // serializeBefore makes the instruction wait in rename until the ROB
        // is empty.

        // In this model, IPR accesses are serialize before
        // instructions, and store conditionals are serialize after
        // instructions.  This is mainly due to lack of support for
        // out-of-order operations of either of those classes of
        // instructions.
        if ((inst->isIprAccess() || inst->isSerializeBefore()) &&
            !inst->isSerializeHandled()) {
            DPRINTF(Rename, "Serialize before instruction encountered.\n");

            if (!inst->isTempSerializeBefore()) {
                renamedSerializing++;
                inst->setSerializeHandled();
            } else {
                renamedTempSerializing++;
            }

            // Change status over to SerializeStall so that other stages know
            // what this is blocked on.
            renameStatus[tid] = SerializeStall;

            serializeInst[tid] = inst;

            blockThisCycle = true;

            break;
        } else if ((inst->isStoreConditional() || inst->isSerializeAfter()) &&
                   !inst->isSerializeHandled()) {
            DPRINTF(Rename, "Serialize after instruction encountered.\n");

            renamedSerializing++;

            inst->setSerializeHandled();

            serializeAfter(insts_to_rename, tid);
        }

		if(isMovInstruction(inst))
		{
			numOfMOVInst++;

			/* if MOVEQ instruction */
			if(isMOVEQ(inst))
			{
				numOfMOVEQ++;
			}
			else if(isMOVNE(inst))
			{
				numOfMOVNE++;
			}

			if(hasTwoOperands(inst) && !hasInstPCReg(inst))
			{
				numOfMOVInstTwoOperands++;

				if(!hasImmediateValueInMov(inst))
				{
					numOfNotImmediateMov++;
					if(hasInstPCReg(inst))
					{
						numOfMovHavingPC++;
					}
				}
				else
				{
					numOfImmediateMov++;
				}
			}
		}

        renameSrcRegs(inst, inst->threadNumber);

        renameDestRegs(inst, inst->threadNumber);

		/* Add move instruction to removeList */
		if(inst->isEliminatedMovInst == true)
		{
			DPRINTF(Rename, "Mov Elimination: move instruction is added into remove list [sn:%i]\n"
					,inst->seqNum);
			cpu->removeFrontInst(inst);
		}

        if (inst->isLoad()) {
                loadsInProgress[tid]++;
        }
        if (inst->isStore()) {
                storesInProgress[tid]++;
        }

		bool isMovInst = isMovInstruction(inst) && hasTwoOperands(inst)
				&& !hasImmediateValueInMov(inst) && !hasInstPCReg(inst);

		/* To execlude increasing renamed_insts of move instruction */
		if(!isMovEliUsed || !isMovInst)
		{
			++renamed_insts;
		}

		/* Eliminate Mov instruction */
		if(isMovEliUsed && isMovInst)
		{
			DPRINTF(Rename, "Mov Elimination: Eliminating MOV instruction [sn:%i]\n",
					inst->seqNum);
			--insts_available;

			// Notify potential listeners that source and destination registers for
			// this instruction have been renamed.
			ppRename->notify(inst);

			continue;
		}

        // Put instruction in rename queue.
        toIEW->insts[toIEWIndex] = inst;
        ++(toIEW->size);

        // Increment which instruction we're on.
        ++toIEWIndex;

        // Decrement how many instructions are available.
        --insts_available;
    }

    instsInProgress[tid] += renamed_insts;
    renameRenamedInsts += renamed_insts;

    // If we wrote to the time buffer, record this.
    if (toIEWIndex) {
        wroteToTimeBuffer = true;
    }

    // Check if there's any instructions left that haven't yet been renamed.
    // If so then block.
    if (insts_available) {
        blockThisCycle = true;
    }

    if (blockThisCycle) {
        block(tid);
        toDecode->renameUnblock[tid] = false;
    }
}

template<class Impl>
void
DefaultRename<Impl>::skidInsert(ThreadID tid)
{
    DynInstPtr inst = NULL;

    while (!insts[tid].empty()) {
        inst = insts[tid].front();

        insts[tid].pop_front();

        assert(tid == inst->threadNumber);

        DPRINTF(Rename, "[tid:%u]: Inserting [sn:%lli] PC: %s into Rename "
                "skidBuffer\n", tid, inst->seqNum, inst->pcState());

        ++renameSkidInsts;

        skidBuffer[tid].push_back(inst);
    }

    if (skidBuffer[tid].size() > skidBufferMax)
    {
        typename InstQueue::iterator it;
        warn("Skidbuffer contents:\n");
        for (it = skidBuffer[tid].begin(); it != skidBuffer[tid].end(); it++)
        {
            warn("[tid:%u]: %s [sn:%i].\n", tid,
                    (*it)->staticInst->disassemble(inst->instAddr()),
                    (*it)->seqNum);
        }
        panic("Skidbuffer Exceeded Max Size");
    }
}

template <class Impl>
void
DefaultRename<Impl>::sortInsts()
{
    int insts_from_decode = fromDecode->size;
    for (int i = 0; i < insts_from_decode; ++i) {
        DynInstPtr inst = fromDecode->insts[i];
        insts[inst->threadNumber].push_back(inst);
#if TRACING_ON
        if (DTRACE(O3PipeView)) {
            inst->renameTick = curTick() - inst->fetchTick;
        }
#endif
    }
}

template<class Impl>
bool
DefaultRename<Impl>::skidsEmpty()
{
    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (!skidBuffer[tid].empty())
            return false;
    }

    return true;
}

template<class Impl>
void
DefaultRename<Impl>::updateStatus()
{
    bool any_unblocking = false;

    list<ThreadID>::iterator threads = activeThreads->begin();
    list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (renameStatus[tid] == Unblocking) {
            any_unblocking = true;
            break;
        }
    }

    // Rename will have activity if it's unblocking.
    if (any_unblocking) {
        if (_status == Inactive) {
            _status = Active;

            DPRINTF(Activity, "Activating stage.\n");

            cpu->activateStage(O3CPU::RenameIdx);
        }
    } else {
        // If it's not unblocking, then rename will not have any internal
        // activity.  Switch it to inactive.
        if (_status == Active) {
            _status = Inactive;
            DPRINTF(Activity, "Deactivating stage.\n");

            cpu->deactivateStage(O3CPU::RenameIdx);
        }
    }
}

template <class Impl>
bool
DefaultRename<Impl>::block(ThreadID tid)
{
    DPRINTF(Rename, "[tid:%u]: Blocking.\n", tid);

    // Add the current inputs onto the skid buffer, so they can be
    // reprocessed when this stage unblocks.
    skidInsert(tid);

    // Only signal backwards to block if the previous stages do not think
    // rename is already blocked.
    if (renameStatus[tid] != Blocked) {
        // If resumeUnblocking is set, we unblocked during the squash,
        // but now we're have unblocking status. We need to tell earlier
        // stages to block.
        if (resumeUnblocking || renameStatus[tid] != Unblocking) {
            toDecode->renameBlock[tid] = true;
            toDecode->renameUnblock[tid] = false;
            wroteToTimeBuffer = true;
        }

        // Rename can not go from SerializeStall to Blocked, otherwise
        // it would not know to complete the serialize stall.
        if (renameStatus[tid] != SerializeStall) {
            // Set status to Blocked.
            renameStatus[tid] = Blocked;
            return true;
        }
    }

    return false;
}

template <class Impl>
bool
DefaultRename<Impl>::unblock(ThreadID tid)
{
    DPRINTF(Rename, "[tid:%u]: Trying to unblock.\n", tid);

    // Rename is done unblocking if the skid buffer is empty.
    if (skidBuffer[tid].empty() && renameStatus[tid] != SerializeStall) {

        DPRINTF(Rename, "[tid:%u]: Done unblocking.\n", tid);

        toDecode->renameUnblock[tid] = true;
        wroteToTimeBuffer = true;

        renameStatus[tid] = Running;
        return true;
    }

    return false;
}

template <class Impl>
void
DefaultRename<Impl>::doSquash(const InstSeqNum &squashed_seq_num, ThreadID tid)
{
    typename std::list<RenameHistory>::iterator hb_it =
        historyBuffer[tid].begin();

    // After a syscall squashes everything, the history buffer may be empty
    // but the ROB may still be squashing instructions.
    if (historyBuffer[tid].empty()) {
        return;
    }

    // Go through the most recent instructions, undoing the mappings
    // they did and freeing up the registers.
    while (!historyBuffer[tid].empty() &&
           hb_it->instSeqNum > squashed_seq_num) {
        assert(hb_it != historyBuffer[tid].end());

        DPRINTF(Rename, "[tid:%u]: Removing history entry with sequence "
                "number %i.\n", tid, hb_it->instSeqNum);

        // Undo the rename mapping only if it was really a change.
        // Special regs that are not really renamed (like misc regs
        // and the zero reg) can be recognized because the new mapping
        // is the same as the old one.  While it would be merely a
        // waste of time to update the rename table, we definitely
        // don't want to put these on the free list.
        if (hb_it->newPhysReg != hb_it->prevPhysReg || hb_it->isMovInst == true) 
		{
			/* Popping MOV instruction from buffer_for_stats */
			if(hb_it->isMovInst)
			{
				assert(!buffer_for_stats.empty());
				buffer_for_stats.pop_front();
			}

            // Tell the rename map to set the architected register to the
            // previous physical register that it was renamed to.
            renameMap[tid]->setEntry(hb_it->archReg, hb_it->prevPhysReg);

            // Put the renamed physical register back on the free list.
			if(!isMovEliUsed || 
					renameMap[tid]->getCountVal(hb_it->newPhysReg) <= 0)
			{
				freeList->addReg(hb_it->newPhysReg);
			}
			else if(renameMap[tid]->getCountVal(hb_it->newPhysReg) > 0)
			{
				renameMap[tid]->decrementCount(hb_it->newPhysReg);
			}
        }

        // Notify potential listeners that the register mapping needs to be
        // removed because the instruction it was mapped to got squashed. Note
        // that this is done before hb_it is incremented.
        ppSquashInRename->notify(std::make_pair(hb_it->instSeqNum,
                                                hb_it->newPhysReg));

        historyBuffer[tid].erase(hb_it++);

        ++renameUndoneMaps;
    }
}

template<class Impl>
void
DefaultRename<Impl>::removeFromHistory(InstSeqNum inst_seq_num, ThreadID tid)
{
    DPRINTF(Rename, "[tid:%u]: Removing a committed instruction from the "
            "history buffer %u (size=%i), until [sn:%lli].\n",
            tid, tid, historyBuffer[tid].size(), inst_seq_num);

    typename std::list<RenameHistory>::iterator hb_it =
        historyBuffer[tid].end();

    --hb_it;

	updateInstSeqNum(inst_seq_num, tid);

    if (historyBuffer[tid].empty()) 
	{
        DPRINTF(Rename, "[tid:%u]: History buffer is empty.\n", tid);
        return;
    } 
	else if (hb_it->instSeqNum > inst_seq_num) 
	{
        DPRINTF(Rename, "[tid:%u]: Old sequence number encountered.  Ensure "
                "that a syscall happened recently.\n", tid);
		DPRINTF(Rename, "History Buffer's seqNum: %d, inst_seq_num: %d.\n",
				hb_it->instSeqNum, inst_seq_num);
        return;
    }

    // Commit all the renames up until (and including) the committed sequence
    // number. Some or even all of the committed instructions may not have
    // rename histories if they did not have destination registers that were
    // renamed.
    while (!historyBuffer[tid].empty() &&
           hb_it != historyBuffer[tid].end() &&
           hb_it->instSeqNum <= inst_seq_num) 
	{

        // Don't free special phys regs like misc and zero regs, which
        // can be recognized because the new mapping is the same as
        // the old one.
        if (hb_it->newPhysReg != hb_it->prevPhysReg || hb_it->isMovInst == true) 
		{
			if(hb_it->isMovInst == true)
			{
				DPRINTF(Rename, "Mov Elimination: Update commitRenameMap "
					"arch reg %d -> phys reg %d [sn:%lli].\n", 
					hb_it->archReg, hb_it->newPhysReg, hb_it->instSeqNum);
				/* Update commit rename map */
				commitRenameMap[tid]->setEntry(hb_it->archReg, hb_it->newPhysReg);

				/* Update committed instruction and microOp */
				assert(!buffer_for_stats.empty());
				DynInstPtr inst = buffer_for_stats.front();
				buffer_for_stats.pop_front();

				if(inst->isCountedForStats == true)
				{
					commit_ptr->updateComInstStats(inst);
				}
			}

			if(!isMovEliUsed || 
					renameMap[tid]->getCountVal(hb_it->prevPhysReg) <= 0)
			{
				/* Add previous phys reg into free list */
				freeList->addReg(hb_it->prevPhysReg);
				DPRINTF(Rename, "[tid:%u]: Freeing up older rename of phy reg %i, "
						"[sn:%lli].\n",
						tid, hb_it->prevPhysReg, hb_it->instSeqNum);
			}
			else if(renameMap[tid]->getCountVal(hb_it->prevPhysReg) > 0)
			{
				DPRINTF(Rename, "Mov Elimination: p%d count: %d\n", 
						hb_it->prevPhysReg, renameMap[tid]->getCountVal(hb_it->prevPhysReg));
				DPRINTF(Rename, "Mov Elimination: Count value is greater than zero [sn:%lli].\n"
						,hb_it->instSeqNum);

				renameMap[tid]->decrementCount(hb_it->prevPhysReg);
			}
        }
		else if(hb_it->newPhysReg == hb_it->prevPhysReg)
		{
			DPRINTF(Rename, "Mov Elimination: newPhysReg p%d == prevPhysReg p%d [sn:%lli]\n",
					hb_it->newPhysReg, hb_it->prevPhysReg, hb_it->instSeqNum);
		}

        ++renameCommittedMaps;

		DPRINTF(Rename, "[tid:%u]: Removing instruction from history buffer (size=%d) [sn:%lli].\n",
				tid, historyBuffer[tid].size()-1, hb_it->instSeqNum);

        historyBuffer[tid].erase(hb_it--);
    }
}

template <class Impl>
inline void
DefaultRename<Impl>::renameSrcRegs(DynInstPtr &inst, ThreadID tid)
{
    ThreadContext *tc = inst->tcBase();
    RenameMap *map = renameMap[tid];
    unsigned num_src_regs = inst->numSrcRegs();

    // Get the architectual register numbers from the source and
    // operands, and redirect them to the right physical register.
    for (int src_idx = 0; src_idx < num_src_regs; src_idx++) {
        RegIndex src_reg = inst->srcRegIdx(src_idx);
        RegIndex rel_src_reg;
        RegIndex flat_rel_src_reg;
        PhysRegIndex renamed_reg;

        switch (regIdxToClass(src_reg, &rel_src_reg)) {
          case IntRegClass:
            flat_rel_src_reg = tc->flattenIntIndex(rel_src_reg);
            renamed_reg = map->lookupInt(flat_rel_src_reg);
            intRenameLookups++;
            break;

          case FloatRegClass:
            flat_rel_src_reg = tc->flattenFloatIndex(rel_src_reg);
            renamed_reg = map->lookupFloat(flat_rel_src_reg);
            fpRenameLookups++;
            break;

          case CCRegClass:
            flat_rel_src_reg = tc->flattenCCIndex(rel_src_reg);
            renamed_reg = map->lookupCC(flat_rel_src_reg);
            break;

          case MiscRegClass:
            // misc regs don't get flattened
            flat_rel_src_reg = rel_src_reg;
            renamed_reg = map->lookupMisc(flat_rel_src_reg);
            break;

          default:
            panic("Reg index is out of bound: %d.", src_reg);
        }

        DPRINTF(Rename, "[tid:%u]: Looking up %s arch reg %i (flattened %i), "
                "got phys reg %i\n", tid, RegClassStrings[regIdxToClass(src_reg)],
                (int)src_reg, (int)flat_rel_src_reg, (int)renamed_reg);

        inst->renameSrcReg(src_idx, renamed_reg);

        // See if the register is ready or not.
        if (scoreboard->getReg(renamed_reg)) {
            DPRINTF(Rename, "[tid:%u]: Register %d is ready.\n",
                    tid, renamed_reg);

            inst->markSrcRegReady(src_idx);
        } else {
            DPRINTF(Rename, "[tid:%u]: Register %d is not ready.\n",
                    tid, renamed_reg);
        }

        ++renameRenameLookups;
    }
}

template <class Impl>
inline void
DefaultRename<Impl>::renameDestRegs(DynInstPtr &inst, ThreadID tid)
{
    ThreadContext *tc = inst->tcBase();
    RenameMap *map = renameMap[tid];
    unsigned num_dest_regs = inst->numDestRegs();

	/* Check if instruction is Mov instruction */
	bool isMovInst = isMovInstruction(inst) && hasTwoOperands(inst) 
		&& !hasImmediateValueInMov(inst) && !hasInstPCReg(inst);

    // Rename the destination registers.
    for (int dest_idx = 0; dest_idx < num_dest_regs; dest_idx++) {
		/* dest_reg: architectural reg index */
        RegIndex dest_reg = inst->destRegIdx(dest_idx);
		/* independent index according to each type */
        RegIndex rel_dest_reg;
		/* flat_rel_dest_reg: architectural reg index?*/
        RegIndex flat_rel_dest_reg;
		/* flat_uni_dest_reg: Unified reg index */
        RegIndex flat_uni_dest_reg;
        typename RenameMap::RenameInfo rename_result;

		/* If Mov elimination is applied */
		if(isMovInst == true && isMovEliUsed)
		{
			/* Set identification of instruction to mov */
			inst->isEliminatedMovInst = true;

			int renamedSrcPhyReg = inst->getSrcRegister(3);
			RegIndex srcArchReg = inst->srcRegIdx(3);
			RegIndex rel_src_reg;

			DPRINTF(Rename, "Mov Elimination: mov r%d <- r%d. [sn:%i]\n",
					dest_reg, srcArchReg, inst->seqNum);
			DPRINTF(Rename, "Mov Elimination: mov p%d <- p%d. [sn:%i]\n",
					renamedSrcPhyReg, renamedSrcPhyReg, inst->seqNum);

//			DPRINTF(Rename, "Data Read: Dest Arch r%d: 0x%x, Src Arch r%d: 0x%x.\n",
//					dest_reg, cpu->readArchIntReg(dest_reg, tid),
//					srcArchReg, cpu->readArchIntReg(srcArchReg, tid));

			switch (regIdxToClass(dest_reg, &rel_dest_reg)) 
			{
			  case IntRegClass:
				flat_rel_dest_reg = tc->flattenIntIndex(rel_dest_reg);
				rename_result = map->renameInt(flat_rel_dest_reg, renamedSrcPhyReg);
				flat_uni_dest_reg = flat_rel_dest_reg;  // 1:1 mapping

				break;

			  case FloatRegClass:
				flat_rel_dest_reg = tc->flattenFloatIndex(rel_dest_reg);
				rename_result = map->renameFloat(flat_rel_dest_reg, renamedSrcPhyReg);
				flat_uni_dest_reg = flat_rel_dest_reg + TheISA::FP_Reg_Base;

				break;

			  case CCRegClass:
				flat_rel_dest_reg = tc->flattenCCIndex(rel_dest_reg);
				rename_result = map->renameCC(flat_rel_dest_reg, renamedSrcPhyReg);
				flat_uni_dest_reg = flat_rel_dest_reg + TheISA::CC_Reg_Base;

				break;

			  case MiscRegClass:
				// misc regs don't get flattened
				flat_rel_dest_reg = rel_dest_reg;
				rename_result = map->renameMisc(flat_rel_dest_reg);
				flat_uni_dest_reg = flat_rel_dest_reg + TheISA::Misc_Reg_Base;
				break;

			  default:
				panic("Reg index is out of bound: %d.", dest_reg);
			}

			/* Count eliminated mov instructions */
			numOfEliminatedInst++;

			/* Access count table and increment count value */
			renameMap[tid]->incrementCount(rename_result.first);
		}
		else
		{
			switch (regIdxToClass(dest_reg, &rel_dest_reg)) 
			{
			  case IntRegClass:
				flat_rel_dest_reg = tc->flattenIntIndex(rel_dest_reg);
				rename_result = map->renameInt(flat_rel_dest_reg);
				flat_uni_dest_reg = flat_rel_dest_reg;  // 1:1 mapping
				break;

			  case FloatRegClass:
				flat_rel_dest_reg = tc->flattenFloatIndex(rel_dest_reg);
				rename_result = map->renameFloat(flat_rel_dest_reg);
				flat_uni_dest_reg = flat_rel_dest_reg + TheISA::FP_Reg_Base;
				break;

			  case CCRegClass:
				flat_rel_dest_reg = tc->flattenCCIndex(rel_dest_reg);
				rename_result = map->renameCC(flat_rel_dest_reg);
				flat_uni_dest_reg = flat_rel_dest_reg + TheISA::CC_Reg_Base;
				break;

			  case MiscRegClass:
				// misc regs don't get flattened
				flat_rel_dest_reg = rel_dest_reg;
				rename_result = map->renameMisc(flat_rel_dest_reg);
				flat_uni_dest_reg = flat_rel_dest_reg + TheISA::Misc_Reg_Base;
				break;

			  default:
				panic("Reg index is out of bound: %d.", dest_reg);
			}
		}

		DPRINTF(Rename, "Mov Elimination: NumFreeEntries: %d [sn:%i]\n",
				renameMap[tid]->numFreeEntries(), inst->seqNum);
		DPRINTF(Rename, "Mov Elimination: IntFreeEntries: %d, FloatFreeEntries: %d\n",
				renameMap[tid]->numIntFreeEntries(), renameMap[tid]->numFloatFreeEntries());

        inst->flattenDestReg(dest_idx, flat_uni_dest_reg);

        // Mark Scoreboard entry as not ready
		if(inst->isEliminatedMovInst != true)
		{
			scoreboard->unsetReg(rename_result.first);
		}

        DPRINTF(Rename, "[tid:%u]: Renaming arch reg %i to physical "
                "reg %i, prev phys reg %i.\n", tid, (int)flat_rel_dest_reg,
                (int)rename_result.first, (int)rename_result.second);

        // Record the rename information so that a history can be kept.
        RenameHistory hb_entry(inst->seqNum, flat_uni_dest_reg, // arch reg
                               rename_result.first,  	// new phys reg
                               rename_result.second, 	// previous physical reg
								inst->isEliminatedMovInst);

        historyBuffer[tid].push_front(hb_entry);

		/* Push instruction to stat buffer */
		if(buffer_for_stats.size() < BUFFER_SIZE && inst->isEliminatedMovInst)
		{
			if(dest_idx == 0)
			{
				inst->isCountedForStats = true;
			}

			buffer_for_stats.push_back(inst);
		}
		else if(buffer_for_stats.size() >= BUFFER_SIZE)
		{
			DPRINTF(Rename, "[tid:%u]: buffer_for_stats size: %d [sn:%i]\n"
					, tid, buffer_for_stats.size(), inst->seqNum);
			assert(buffer_for_stats.size() < BUFFER_SIZE);
		}

        DPRINTF(Rename, "[tid:%u]: Adding instruction to history buffer "
                "(size=%i), [sn:%lli].\n",tid,
                historyBuffer[tid].size(),
                (*historyBuffer[tid].begin()).instSeqNum);

        // Tell the instruction to rename the appropriate destination
        // register (dest_idx) to the new physical register
        // (rename_result.first), and record the previous physical
        // register that the same logical register was renamed to
        // (rename_result.second).
        inst->renameDestReg(dest_idx,
                            rename_result.first,
                            rename_result.second);

        ++renameRenamedOperands;
    }
}

template <class Impl>
inline int
DefaultRename<Impl>::calcFreeROBEntries(ThreadID tid)
{
    int num_free = freeEntries[tid].robEntries -
                  (instsInProgress[tid] - fromIEW->iewInfo[tid].dispatched);

	DPRINTF(Rename, "calcFreeROBEntires: free robEntries: %d, instsInProgress: %d, "
			"dispatched Insts: %d\n", freeEntries[tid].robEntries, instsInProgress[tid],
			fromIEW->iewInfo[tid].dispatched);
    DPRINTF(Rename,"[tid:%i]: %i rob free\n",tid,num_free);

    return num_free;
}

template <class Impl>
inline int
DefaultRename<Impl>::calcFreeIQEntries(ThreadID tid)
{
    int num_free = freeEntries[tid].iqEntries -
                  (instsInProgress[tid] - fromIEW->iewInfo[tid].dispatched);

	DPRINTF(Rename, "calcFreeIQEntires: free iqEntries: %d, instsInProgress: %d, "
			"dispatched Insts: %d\n", freeEntries[tid].iqEntries, instsInProgress[tid],
			fromIEW->iewInfo[tid].dispatched);
	DPRINTF(Rename,"[tid:%i]: %i iq free\n",tid,num_free);

    return num_free;
}

template <class Impl>
inline int
DefaultRename<Impl>::calcFreeLQEntries(ThreadID tid)
{
        int num_free = freeEntries[tid].lqEntries -
                                  (loadsInProgress[tid] - fromIEW->iewInfo[tid].dispatchedToLQ);
        DPRINTF(Rename, "calcFreeLQEntries: free lqEntries: %d, loadsInProgress: %d, "
                "loads dispatchedToLQ: %d\n", freeEntries[tid].lqEntries,
                loadsInProgress[tid], fromIEW->iewInfo[tid].dispatchedToLQ);
        return num_free;
}

template <class Impl>
inline int
DefaultRename<Impl>::calcFreeSQEntries(ThreadID tid)
{
        int num_free = freeEntries[tid].sqEntries -
                                  (storesInProgress[tid] - fromIEW->iewInfo[tid].dispatchedToSQ);
        DPRINTF(Rename, "calcFreeSQEntries: free sqEntries: %d, storesInProgress: %d, "
                "stores dispatchedToSQ: %d\n", freeEntries[tid].sqEntries,
                storesInProgress[tid], fromIEW->iewInfo[tid].dispatchedToSQ);
        return num_free;
}

template <class Impl>
unsigned
DefaultRename<Impl>::validInsts()
{
    unsigned inst_count = 0;

    for (int i=0; i<fromDecode->size; i++) {
        if (!fromDecode->insts[i]->isSquashed())
            inst_count++;
    }

    return inst_count;
}

template <class Impl>
void
DefaultRename<Impl>::readStallSignals(ThreadID tid)
{
    if (fromIEW->iewBlock[tid]) {
        stalls[tid].iew = true;
    }

    if (fromIEW->iewUnblock[tid]) {
        assert(stalls[tid].iew);
        stalls[tid].iew = false;
    }
}

template <class Impl>
bool
DefaultRename<Impl>::checkStall(ThreadID tid)
{
    bool ret_val = false;

    if (stalls[tid].iew) {
        DPRINTF(Rename,"[tid:%i]: Stall from IEW stage detected.\n", tid);
        ret_val = true;
    } else if (calcFreeROBEntries(tid) <= 0) {
        DPRINTF(Rename,"[tid:%i]: Stall: ROB has 0 free entries.\n", tid);
        ret_val = true;
    } else if (calcFreeIQEntries(tid) <= 0) {
        DPRINTF(Rename,"[tid:%i]: Stall: IQ has 0 free entries.\n", tid);
        ret_val = true;
    } else if (calcFreeLQEntries(tid) <= 0 && calcFreeSQEntries(tid) <= 0) {
        DPRINTF(Rename,"[tid:%i]: Stall: LSQ has 0 free entries.\n", tid);
        ret_val = true;
    } else if (renameMap[tid]->numFreeEntries() <= 0) {
        DPRINTF(Rename,"[tid:%i]: Stall: RenameMap has 0 free entries.\n", tid);
        ret_val = true;
    } else if (renameStatus[tid] == SerializeStall &&
               (!emptyROB[tid] || instsInProgress[tid])) {
        DPRINTF(Rename,"[tid:%i]: Stall: Serialize stall and ROB is not "
                "empty.\n",
                tid);
        ret_val = true;
    }

    return ret_val;
}

template <class Impl>
void
DefaultRename<Impl>::readFreeEntries(ThreadID tid)
{
    if (fromIEW->iewInfo[tid].usedIQ)
        freeEntries[tid].iqEntries = fromIEW->iewInfo[tid].freeIQEntries;

    if (fromIEW->iewInfo[tid].usedLSQ) {
        freeEntries[tid].lqEntries = fromIEW->iewInfo[tid].freeLQEntries;
        freeEntries[tid].sqEntries = fromIEW->iewInfo[tid].freeSQEntries;
    }

    if (fromCommit->commitInfo[tid].usedROB) {
        freeEntries[tid].robEntries =
            fromCommit->commitInfo[tid].freeROBEntries;
        emptyROB[tid] = fromCommit->commitInfo[tid].emptyROB;
    }

    DPRINTF(Rename, "[tid:%i]: Free IQ: %i, Free ROB: %i, "
                    "Free LQ: %i, Free SQ: %i\n",
            tid,
            freeEntries[tid].iqEntries,
            freeEntries[tid].robEntries,
            freeEntries[tid].lqEntries,
            freeEntries[tid].sqEntries);

    DPRINTF(Rename, "[tid:%i]: %i instructions not yet in ROB\n",
            tid, instsInProgress[tid]);
}

template <class Impl>
bool
DefaultRename<Impl>::checkSignalsAndUpdate(ThreadID tid)
{
    // Check if there's a squash signal, squash if there is
    // Check stall signals, block if necessary.
    // If status was blocked
    //     check if stall conditions have passed
    //         if so then go to unblocking
    // If status was Squashing
    //     check if squashing is not high.  Switch to running this cycle.
    // If status was serialize stall
    //     check if ROB is empty and no insts are in flight to the ROB

    readFreeEntries(tid);
    readStallSignals(tid);

    if (fromCommit->commitInfo[tid].squash) {
        DPRINTF(Rename, "[tid:%u]: Squashing instructions due to squash from "
                "commit.\n", tid);

        squash(fromCommit->commitInfo[tid].doneSeqNum, tid);

        return true;
    }

    if (checkStall(tid)) {
        return block(tid);
    }

    if (renameStatus[tid] == Blocked) {
        DPRINTF(Rename, "[tid:%u]: Done blocking, switching to unblocking.\n",
                tid);

        renameStatus[tid] = Unblocking;

        unblock(tid);

        return true;
    }

    if (renameStatus[tid] == Squashing) {
        // Switch status to running if rename isn't being told to block or
        // squash this cycle.
        if (resumeSerialize) {
            DPRINTF(Rename, "[tid:%u]: Done squashing, switching to serialize.\n",
                    tid);

            renameStatus[tid] = SerializeStall;
            return true;
        } else if (resumeUnblocking) {
            DPRINTF(Rename, "[tid:%u]: Done squashing, switching to unblocking.\n",
                    tid);
            renameStatus[tid] = Unblocking;
            return true;
        } else {
            DPRINTF(Rename, "[tid:%u]: Done squashing, switching to running.\n",
                    tid);

            renameStatus[tid] = Running;
            return false;
        }
    }

    if (renameStatus[tid] == SerializeStall) {
        // Stall ends once the ROB is free.
        DPRINTF(Rename, "[tid:%u]: Done with serialize stall, switching to "
                "unblocking.\n", tid);

        DynInstPtr serial_inst = serializeInst[tid];

        renameStatus[tid] = Unblocking;

        unblock(tid);

        DPRINTF(Rename, "[tid:%u]: Processing instruction [%lli] with "
                "PC %s.\n", tid, serial_inst->seqNum, serial_inst->pcState());

        // Put instruction into queue here.
        serial_inst->clearSerializeBefore();

        if (!skidBuffer[tid].empty()) {
            skidBuffer[tid].push_front(serial_inst);
        } else {
            insts[tid].push_front(serial_inst);
        }

        DPRINTF(Rename, "[tid:%u]: Instruction must be processed by rename."
                " Adding to front of list.\n", tid);

        serializeInst[tid] = NULL;

        return true;
    }

    // If we've reached this point, we have not gotten any signals that
    // cause rename to change its status.  Rename remains the same as before.
    return false;
}

template<class Impl>
void
DefaultRename<Impl>::serializeAfter(InstQueue &inst_list, ThreadID tid)
{
    if (inst_list.empty()) {
        // Mark a bit to say that I must serialize on the next instruction.
        serializeOnNextInst[tid] = true;
        return;
    }

    // Set the next instruction as serializing.
    inst_list.front()->setSerializeBefore();
}

template <class Impl>
inline void
DefaultRename<Impl>::incrFullStat(const FullSource &source)
{
    switch (source) {
      case ROB:
        ++renameROBFullEvents;
        break;
      case IQ:
        ++renameIQFullEvents;
        break;
      case LQ:
        ++renameLQFullEvents;
        break;
      case SQ:
        ++renameSQFullEvents;
        break;
      default:
        panic("Rename full stall stat should be incremented for a reason!");
        break;
    }
}

template <class Impl>
void
DefaultRename<Impl>::dumpHistory()
{
    typename std::list<RenameHistory>::iterator buf_it;

    for (ThreadID tid = 0; tid < numThreads; tid++) {

        buf_it = historyBuffer[tid].begin();

        while (buf_it != historyBuffer[tid].end()) {
            cprintf("Seq num: %i\nArch reg: %i New phys reg: %i Old phys "
                    "reg: %i\n", (*buf_it).instSeqNum, (int)(*buf_it).archReg,
                    (int)(*buf_it).newPhysReg, (int)(*buf_it).prevPhysReg);

            buf_it++;
        }
    }
}

template <typename Impl>
string DefaultRename<Impl>::getOpString(string strTarget, string strTok)
{
	int nCutPos;
	string strResult;

	while((nCutPos = strTarget.find_first_of(strTok)) != strTarget.npos)
	{
		if(nCutPos > 0)
		{
			strResult = strTarget.substr(0, nCutPos);
			break;
		}
		strTarget = strTarget.substr(nCutPos+1);
	}

	return strResult;
}

template <typename Impl>
bool DefaultRename<Impl>::hasTwoOperands(DynInstPtr &inst)
{
	string inst_disassembly = inst->staticInst->disassemble(inst->instAddr());
	int nCutPos;
	int num_of_operands = 0;

	while((nCutPos = inst_disassembly.find_first_of(" ")) 
			!= inst_disassembly.npos)
	{
		if(nCutPos > 0)
		{
			num_of_operands++;
		}
		inst_disassembly = inst_disassembly.substr(nCutPos+1);
	}

	if(inst_disassembly.length() > 0)
	{
		num_of_operands++;
	}

	if(num_of_operands == 3)
	{
		DPRINTF(Rename, "MOV Elimination: mov instruction (%s) has only two operands [sn:%i]\n"
				, inst->staticInst->disassemble(inst->instAddr()), inst->seqNum);
		return true;
	}
	else
		return false;
		
}

template <typename Impl>
bool DefaultRename<Impl>::isMovInstruction(DynInstPtr &inst)
{
	string inst_disassembly = inst->staticInst->disassemble(inst->instAddr());
	string op_string = getOpString(inst_disassembly, " ");

	/* Count instruction relative to mov, such as movcc, movls, moveq.... */
	if(inst_disassembly.find("mov") != string::npos)
	{
	}

	if(op_string.compare("mov") == 0)
	{
//		DPRINTF(Rename, "MOV Elimination: instruction (%s) corresponds MOV [sn:%i]\n"
//				, inst_disassembly, inst->seqNum);
		return true;
	}
	else
		return false;
}

template <typename Impl>
bool DefaultRename<Impl>::hasImmediateValueInMov(DynInstPtr &inst)
{
	string inst_disassembly = inst->staticInst->disassemble(inst->instAddr());

	if(inst_disassembly.find("mov") != string::npos &&
			inst_disassembly.find("#") != string::npos)
	{
		return true;
	}
	else
		return false;
}

template <typename Impl>
bool DefaultRename<Impl>::isNewStateReg(RegIndex arch_reg_index, ThreadID tid)
{
	ThreadContext *tc = cpu->getContext(tid);
	RenameMap *map = renameMap[tid];
	RegIndex rel_reg;
	RegIndex flat_rel_reg;
	bool result = false;

	switch (regIdxToClass(arch_reg_index, &rel_reg)) 
	{
	  case IntRegClass:
		flat_rel_reg = tc->flattenIntIndex(rel_reg);
		result = map->isNewStateInt(flat_rel_reg);
		break;

	  case FloatRegClass:
		flat_rel_reg = tc->flattenFloatIndex(rel_reg);
		result = map->isNewStateFloat(flat_rel_reg);
		break;

	  case CCRegClass:
		flat_rel_reg = tc->flattenCCIndex(rel_reg);
		result = map->isNewStateCC(flat_rel_reg);
		break;

	  case MiscRegClass:
		// misc regs don't get flattened
		flat_rel_reg = rel_reg;
		break;

	  default:
		panic("Reg index is out of bound: %d.", arch_reg_index);
	}

	if(result == false)
	{
		DPRINTF(Rename, "Mov Elimination: arch reg %d is Old state.\n"
				, arch_reg_index);
	}
	else
	{
		DPRINTF(Rename, "Mov Elimination: arch reg %d is New state.\n"
				, arch_reg_index);
	}

	return result;
}

template <typename Impl>
void DefaultRename<Impl>::setMapNew(RegIndex arch_reg_index, ThreadID tid)
{
	ThreadContext *tc = cpu->getContext(tid);
	RenameMap *map = renameMap[tid];
	RegIndex rel_reg;
	RegIndex flat_rel_reg;

	DPRINTF(Rename, "Mov Elimination: arch reg %d sets New state.\n",
			arch_reg_index);

	switch (regIdxToClass(arch_reg_index, &rel_reg)) 
	{
	  case IntRegClass:
		flat_rel_reg = tc->flattenIntIndex(rel_reg);
		map->setIntMapNew(flat_rel_reg);
		break;

	  case FloatRegClass:
		flat_rel_reg = tc->flattenFloatIndex(rel_reg);
		map->setFloatMapNew(flat_rel_reg);
		break;

	  case CCRegClass:
		flat_rel_reg = tc->flattenCCIndex(rel_reg);
		map->setCCMapNew(flat_rel_reg);
		break;

	  case MiscRegClass:
		// misc regs don't get flattened
		flat_rel_reg = rel_reg;
		break;

	  default:
		panic("Reg index is out of bound: %d.", arch_reg_index);
	}
}

template <typename Impl>
void DefaultRename<Impl>::
setRegOldStateInHB(RegIndex src_arch_reg_index, ThreadID tid)
{
	/* Search if there is register we want */
    typename std::list<RenameHistory>::iterator buf_it = historyBuffer[tid].begin();
	int count = 0;

	while(buf_it != historyBuffer[tid].end())
	{
		if((*buf_it).archReg == src_arch_reg_index)
		{
			DPRINTF(Rename, "Mov Elimination: searching arch reg %d is complete.\n",
					src_arch_reg_index);
			DPRINTF(Rename, "Mov Elimination: set arch reg %d to old state.\n",
					src_arch_reg_index);
			
			(*buf_it).isMovInst = false;
			count++;
		}

		buf_it++;
	}

	if(count == 0)
	{
		DPRINTF(Rename, "Mov Elimination: There is no matching arch reg in history buffer.\n");
		assert(count == 0);
	}
}

template <typename Impl>
void DefaultRename<Impl>::setRegNewStateInHB(RegIndex src_arch_reg, ThreadID tid)
{
	/* Search if there is register we want */
    typename std::list<RenameHistory>::iterator buf_it = historyBuffer[tid].begin();
	int count = 0;

	while(buf_it != historyBuffer[tid].end())
	{
		if(buf_it->archReg == src_arch_reg)
		{
			DPRINTF(Rename, "Mov Elimination: searching arch reg %d [sn:%i]is complete.\n",
					src_arch_reg, buf_it->instSeqNum);
			
			(*buf_it).isMovInst = true;
			count++;
		}

		buf_it++;
	}

	if(count == 0)
	{
		DPRINTF(Rename, "Mov Elimination: There is no matching arch reg in history buffer.\n");
		assert(count == 0);
	}
}

template <typename Impl>
bool DefaultRename<Impl>::isDestRegPCReg(DynInstPtr &inst)
{
    unsigned num_dest_regs = inst->numDestRegs();
	bool result = false;

    for (int dest_idx = 0; dest_idx < num_dest_regs; dest_idx++) 
	{
		/* dest_reg: architectural reg index */
        RegIndex dest_reg = inst->destRegIdx(dest_idx);

		/* If dest reg is PC register */
		if(dest_reg == 15)
		{
			DPRINTF(Rename, "Mov Elimination: Dest Reg corresponds PC reg (%d) [sn:%i]\n",
					dest_reg, inst->seqNum);
			return true;
		}
	}

	return result;
}

template <typename Impl>
bool DefaultRename<Impl>::hasInstPCReg(DynInstPtr &inst)
{
	bool result = false;
	string inst_disassembly = inst->staticInst->disassemble(inst->instAddr());
	string op_string = getOpString(inst_disassembly, " ");

	/* Count instruction relative to mov, such as movcc, movls, moveq.... */
	if(inst_disassembly.find("pc") != string::npos)
	{
		DPRINTF(Rename, "Mov Elimination: Inst has PC reg of src [sn:%i]\n",
				inst->seqNum);
		return true;
	}

	return result;
}

template <typename Impl>
void DefaultRename<Impl>::updateInstSeqNum(InstSeqNum &inst_seq_num, ThreadID tid)
{
	typename std::list<RenameHistory>::iterator hb_iter =
			historyBuffer[tid].begin();
	bool end_flag = false;

	/* Get pointer of the inst seq num */
	while(hb_iter != historyBuffer[tid].end())
	{
		if(hb_iter->instSeqNum == inst_seq_num)
			break;

		hb_iter++;
	}

	if(hb_iter == historyBuffer[tid].end())
		return;

	hb_iter--;

	while(end_flag == false && hb_iter->isMovInst == true)
	{
		if(hb_iter == historyBuffer[tid].begin())
			end_flag = true;

		inst_seq_num = hb_iter->instSeqNum;	

		DPRINTF(Rename, "Update inst seq num [sn:%lli].\n", inst_seq_num);

		/* Decrement iterator */
		hb_iter--;
	}
}

template <typename Impl>
bool DefaultRename<Impl>::isMOVEQ(DynInstPtr &inst)
{
	string inst_disassembly = inst->staticInst->disassemble(inst->instAddr());
	string op_string = getOpString(inst_disassembly, " ");

	if(op_string.compare("moveq") == 0)
	{
		return true;
	}
	else
		return false;
}

template <typename Impl>
bool DefaultRename<Impl>::isMOVNE(DynInstPtr &inst)
{
	string inst_disassembly = inst->staticInst->disassemble(inst->instAddr());
	string op_string = getOpString(inst_disassembly, " ");

	if(op_string.compare("movne") == 0)
	{
		return true;
	}
	else
		return false;
}

#endif//__CPU_O3_RENAME_IMPL_HH__
