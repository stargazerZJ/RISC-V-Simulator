//
// Created by zj on 8/1/2024.
//

#pragma once

#include "fetcher.h"
#include "memory.h"
#include "regfile.h"
#include "rs_alu.h"
#include "rs_bcu.h"
#include "rs_mem.h"
#include "reorder_buffer.h"
#include "decoder.h"
#include "tools.h"
#include "stats.h"
#include <iostream>

class Simulator {
public:
    Simulator() : memory_(std::make_unique<Memory>()), fetcher_(memory_.get()), mem_(memory_.get()),
                  reorder_buffer_(&stats_) {
        // Add modules to the CPU
        cpu_.add_module(&fetcher_);
        cpu_.add_module(&decoder_);
        cpu_.add_module(&rs_alu_);
        cpu_.add_module(&alu_);
        cpu_.add_module(&rs_bcu_);
        cpu_.add_module(&bcu_);
        cpu_.add_module(&rs_mem_);
        cpu_.add_module(&mem_);
        cpu_.add_module(&reg_file_);
        cpu_.add_module(&reorder_buffer_);

        // Connecting the modules

        // To Fetcher
        // Fetcher -> Fetcher
        fetcher_.last_PC_plus_4 = [&] { return fetcher_.program_counter + 4; };
        // Decoder -> Fetcher
        fetcher_.pc_from_decoder         = decoder_.to_fetcher.pc;
        fetcher_.pc_from_decoder_enabled = decoder_.to_fetcher.enabled;
        // ROB -> Fetcher
        fetcher_.pc_from_ROB           = reorder_buffer_.to_fetcher.pc;
        fetcher_.pc_from_ROB_enabled   = reorder_buffer_.to_fetcher.pc_enabled;
        fetcher_.pc_of_branch          = reorder_buffer_.to_fetcher.branch_pc;
        fetcher_.branch_taken          = reorder_buffer_.to_fetcher.branch_taken;
        fetcher_.branch_record_enabled = reorder_buffer_.to_fetcher.branch_record_enabled;

        // To Decoder
        // Fetcher -> Decoder
        decoder_.from_fetcher.instruction            = fetcher_.instruction;
        decoder_.from_fetcher.program_counter        = fetcher_.program_counter;
        decoder_.from_fetcher.predicted_branch_taken = fetcher_.predicted_branch_taken;
        // CDB -> Decoder
        dark::connect(decoder_.cdb_input_alu, alu_.cdb_output);
        dark::connect(decoder_.cdb_input_mem, mem_.cdb_output);
        // RegFile -> Decoder
        dark::connect(decoder_.from_regfile, static_cast<regfile::RegFile_Output&>(reg_file_));
        // Reservation Stations -> Decoder
        decoder_.rs_alu_full = [&] {
            return (rs_alu_.vacancy == 0) || (rs_alu_.vacancy == 1 && decoder_.to_rs_alu.enabled == 1);
        };
        decoder_.rs_bcu_full = [&] {
            return (rs_bcu_.vacancy == 0) || (rs_bcu_.vacancy == 1 && decoder_.to_rs_bcu.enabled == 1);
        };
        decoder_.rs_mem_load_full = [&] {
            return (rs_mem_.load_vacancy == 0) || (rs_mem_.load_vacancy == 1 && decoder_.to_rs_mem_load.enabled == 1);
        };
        decoder_.rs_mem_store_full = [&] {
            return (rs_mem_.store_vacancy == 0) || (rs_mem_.store_vacancy == 1 && decoder_.to_rs_mem_store.enabled ==
                1);
        };
        // ROB -> Decoder
        dark::connect(decoder_.from_rob, reorder_buffer_.to_decoder);
        decoder_.rob_full = [&] {
            return (reorder_buffer_.vacancy == 0) || (reorder_buffer_.vacancy == 1 && decoder_.to_rob.enabled == 1);
        };
        decoder_.rob_id = [&] {
            return decoder_.to_rob.enabled == 1
                       ? rob::ROB::next_tail(to_unsigned(reorder_buffer_.next_tail_output))
                       : to_unsigned(reorder_buffer_.next_tail_output);
        };
        dark::connect(decoder_.commit_info, reorder_buffer_.commit_output);
        decoder_.flush_input = reorder_buffer_.flush_output;

        // To RS_ALU
        dark::connect(rs_alu_.operation_input, decoder_.to_rs_alu);
        dark::connect(rs_alu_.cdb_input_alu, alu_.cdb_output);
        dark::connect(rs_alu_.cdb_input_mem, mem_.cdb_output);
        rs_alu_.flush_input = reorder_buffer_.flush_output;

        // To ALU
        alu_.dest = [&] {
            return reorder_buffer_.flush_output == 1 ? 0 : to_unsigned(rs_alu_.to_alu.dest);
        };
        alu_.op  = rs_alu_.to_alu.op;
        alu_.rs1 = rs_alu_.to_alu.Vj;
        alu_.rs2 = rs_alu_.to_alu.Vk;

        // To RS_BCU
        dark::connect(rs_bcu_.operation_input, decoder_.to_rs_bcu);
        dark::connect(rs_bcu_.cdb_input_alu, alu_.cdb_output);
        dark::connect(rs_bcu_.cdb_input_mem, mem_.cdb_output);
        rs_bcu_.flush_input = reorder_buffer_.flush_output;

        // To BCU
        bcu_.dest = [&] {
            return reorder_buffer_.flush_output == 1 ? 0 : to_unsigned(rs_bcu_.to_bcu.dest);
        };
        bcu_.op             = rs_bcu_.to_bcu.op;
        bcu_.rs1            = rs_bcu_.to_bcu.Vj;
        bcu_.rs2            = rs_bcu_.to_bcu.Vk;
        bcu_.pc_fallthrough = rs_bcu_.to_bcu.pc_fallthrough;
        bcu_.pc_target      = rs_bcu_.to_bcu.pc_target;


        // To RS_Mem
        dark::connect(rs_mem_.load_input, decoder_.to_rs_mem_load);
        dark::connect(rs_mem_.store_input, decoder_.to_rs_mem_store);
        dark::connect(rs_mem_.cdb_input_alu, alu_.cdb_output);
        dark::connect(rs_mem_.cdb_input_mem, mem_.cdb_output);
        rs_mem_.flush_input = reorder_buffer_.flush_output;
        dark::connect(rs_mem_.rob_commit, reorder_buffer_.commit_output);
        rs_mem_.recv = mem_.recv;

        // To Mem
        dark::connect(mem_.operation_input, rs_mem_.to_mem);
        mem_.flush_input = reorder_buffer_.flush_output;

        // To RegFile
        dark::connect(reg_file_.from_decoder, decoder_.to_reg_file);
        dark::connect(reg_file_.from_rob, reorder_buffer_.to_reg_file);
        reg_file_.flush_input = reorder_buffer_.flush_output;

        // To ROB
        dark::connect(reorder_buffer_.operation_input, decoder_.to_rob);
        dark::connect(reorder_buffer_.cdb_input_alu, alu_.cdb_output);
        dark::connect(reorder_buffer_.cdb_input_mem, mem_.cdb_output);
        dark::connect(reorder_buffer_.bcu_input, static_cast<RS_BCU::BCU_Output&>(bcu_));
    }

    void run() {
        std::ios_base::sync_with_stdio(false);
        memory_->load_data(std::cin);

        std::function halt_callback = [&] {
            unsigned int output = reg_file_.get_data(10) & 0xFF;
            auto cpu_cycle_count = cpu_.get_cycle_count();
            stats_.report(cpu_cycle_count);
            std::cout << output << std::endl;
            exit(0);
        };

        reorder_buffer_.halt_callback = halt_callback;

        cpu_.run(1e9, true);

        dark::debug::assert(false, "CPU: maxmimum cycle count reached");
    }

private:
    std::unique_ptr<Memory>     memory_;
    fetcher::Fetcher            fetcher_;
    decoder::Decoder            decoder_;
    RS_ALU::Reservation_Station rs_alu_;
    RS_ALU::ALU                 alu_;
    RS_BCU::Reservation_Station rs_bcu_;
    RS_BCU::BCU                 bcu_;
    RS_Mem::Reservation_Station rs_mem_;
    RS_Mem::MemoryUnit          mem_;
    regfile::RegFile            reg_file_;
    rob::ROB                    reorder_buffer_;
    dark::CPU                   cpu_;
    Stats                       stats_;
};
