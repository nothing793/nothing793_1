`include "core_struct.vh"
module Core (
    input clk,
    input rst,

    Mem_ift.Master imem_ift,
    Mem_ift.Master dmem_ift,

    output cosim_valid,
    output CorePack::CoreInfo cosim_core_info
);
    import CorePack::*;
    
    logic [63:0] pc, next_pc, pc_plus4;
    assign pc_plus4=pc+64'd4;

    inst_t inst;
    logic [4:0]rs1,rs2,rd;
    data_t read_data_1,read_data_2;

    logic we_reg, we_mem, re_mem, npc_sel;
    imm_op_enum       immgen_op;
    alu_op_enum        alu_op;
    cmp_op_enum        cmp_op;
    alu_asel_op_enum   alu_asel;
    alu_bsel_op_enum   alu_bsel;
    wb_sel_op_enum     wb_sel;
    mem_op_enum        mem_op;

    data_t imm;

    
    data_t dmem_wdata,dmem_rdata;
    addr_t dmem_waddr,dmem_raddr;
    mask_t dmem_wmask;

    data_t alu_res,alu_a,alu_b;
    logic cmp_res;
    logic br_taken;
    
    data_t mem_read_extended;//写回数据
    data_t wb_val;

always_ff @( posedge clk or posedge rst ) begin 
    if(rst)
        pc<=64'h0000_0000;
    else 
        pc<=next_pc;
end

assign imem_ift.r_request_valid=1'b1;
assign imem_ift.r_request_bits.raddr = {pc[63:3], 3'b0};  // 8-byte aligned
assign inst = pc[2] ? imem_ift.r_reply_bits.rdata[63:32] : imem_ift.r_reply_bits.rdata[31:0];

assign rs1=inst[19:15];     //译码
assign rs2=inst[24:20];
assign rd=inst[11:7];

controller  u_controller(
    .inst(inst),
    .we_reg(we_reg),
    .we_mem(we_mem),
    .re_mem(re_mem),
    .npc_sel(npc_sel),
    .immgen_op(immgen_op),
    .alu_op(alu_op),
    .cmp_op(cmp_op),
    .alu_asel(alu_asel),
    .alu_bsel(alu_bsel),
    .wb_sel(wb_sel),
    .mem_op(mem_op)
    );

always_comb begin
    case (immgen_op)
        I_IMM:imm={{52{inst[31]}},inst[31:20]};
        S_IMM:imm={{52{inst[31]}},inst[31:25],inst[11:7]};
        B_IMM:imm={{52{inst[31]}},inst[7],inst[30:25],inst[11:8],1'b0};
        U_IMM:imm={{32{inst[31]}},inst[31:12],12'b0};
        UJ_IMM:imm={{43{inst[31]}},inst[31],inst[19:12],inst[20],inst[30:21],1'b0};
        default: imm=64'b0;
    endcase
end

RegFile u_regfile(
  .clk(clk),
  .rst(rst),
  .we(we_reg),
  .read_addr_1(rs1),
  .read_addr_2(rs2),
  .write_addr(rd),
  .write_data(wb_val),
  .read_data_1(read_data_1),
  .read_data_2(read_data_2)
);

always_comb begin 
    case (alu_asel)
        ASEL_REG: alu_a=read_data_1;
        ASEL_PC: alu_a=pc;
        ASEL0: alu_a=64'b0;
        default: alu_a=read_data_1;
    endcase
end
always_comb begin
    case (alu_bsel)
        BSEL_REG: alu_b=read_data_2;
        BSEL_IMM: alu_b=imm;
        default: alu_b=read_data_2;
    endcase
end

ALU u_alu(
    .a(alu_a),
    .b(alu_b),
    .alu_op(alu_op),
    .res(alu_res)
    );

Cmp u_cmp(
    .a(read_data_1),
    .b(read_data_2),
    .cmp_op(cmp_op),
    .cmp_res(cmp_res)
    );

wire is_branch=(inst[6:0]==BRANCH_OPCODE);
wire is_jump=(inst[6:0]==JAL_OPCODE||inst[6:0]==JALR_OPCODE);
assign br_taken=(is_branch && cmp_res)||(is_jump);

wire is_halt = (inst == 32'h00000000) || (inst == 32'h00100073);

assign next_pc = is_halt ? pc : (br_taken ? alu_res : pc_plus4);

//数据储存接口

//读请求
assign dmem_ift.r_request_valid=re_mem;
assign dmem_ift.r_request_bits.raddr=alu_res;
//读响应
assign dmem_rdata=dmem_ift.r_reply_bits.rdata;

//写请求
assign dmem_ift.w_request_valid=we_mem;
assign dmem_ift.w_request_bits.waddr=alu_res;
assign dmem_ift.w_request_bits.wdata=dmem_wdata;
assign dmem_ift.w_request_bits.wmask=dmem_wmask;

DataPkg u_datapkg(
    .mem_op(mem_op),
    .reg_data(read_data_2),
    .dmem_waddr(alu_res),//低三位代表字节索引
    .dmem_wdata(dmem_wdata)
    );

DataTrunc u_datatrunc(
    .dmem_rdata(dmem_rdata),
    .mem_op(mem_op),
    .dmem_raddr(alu_res),
    .read_data(mem_read_extended)
    );

MaskGen u_maskgen(
    .mem_op(mem_op),
    .dmem_waddr(alu_res),
    .dmem_wmask(dmem_wmask)
    );
        
always_comb begin 
    case (wb_sel)
        WB_SEL_ALU:wb_val=alu_res;
        WB_SEL_MEM:wb_val=mem_read_extended;
        WB_SEL_PC:wb_val=pc_plus4; 
        default: wb_val=alu_res;
    endcase
end
    
    assign cosim_valid = ~rst;
    assign cosim_core_info.pc        = pc;
    assign cosim_core_info.inst      = {32'b0,inst};   
    assign cosim_core_info.rs1_id    = {59'b0, rs1};
    assign cosim_core_info.rs1_data  = read_data_1;
    assign cosim_core_info.rs2_id    = {59'b0, rs2};
    assign cosim_core_info.rs2_data  = read_data_2;
    assign cosim_core_info.alu       = alu_res;
    assign cosim_core_info.mem_addr  = dmem_ift.r_request_bits.raddr;
    assign cosim_core_info.mem_we    = {63'b0, dmem_ift.w_request_valid};
    assign cosim_core_info.mem_wdata = dmem_ift.w_request_bits.wdata;
    assign cosim_core_info.mem_rdata = dmem_ift.r_reply_bits.rdata;
    assign cosim_core_info.rd_we     = {63'b0, we_reg};
    assign cosim_core_info.rd_id     = {59'b0, rd}; 
    assign cosim_core_info.rd_data   = wb_val;
    assign cosim_core_info.br_taken  = {63'b0, br_taken};
    assign cosim_core_info.npc       = next_pc;

endmodule

module MultiFSM(
    input clk,
    input rst,
    Mem_ift.Master imem_ift,
    Mem_ift.Master dmem_ift,
    input we_mem,
    input re_mem,
    input CorePack::addr_t pc,
    input CorePack::addr_t alu_res,
    input CorePack::data_t data_package,
    input CorePack::mask_t mask_package,
    output stall
);
    import CorePack::*;

    // fill your code for bonus

endmodule