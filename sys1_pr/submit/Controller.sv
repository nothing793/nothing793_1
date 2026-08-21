`include "core_struct.vh"
module controller (
    input CorePack::inst_t inst,
    output logic we_reg,
    output logic we_mem,
    output logic re_mem,
    output logic npc_sel,
    output CorePack::imm_op_enum immgen_op,
    output CorePack::alu_op_enum alu_op,
    output CorePack::cmp_op_enum cmp_op,
    output CorePack::alu_asel_op_enum alu_asel,
    output CorePack::alu_bsel_op_enum alu_bsel,
    output CorePack::wb_sel_op_enum wb_sel,
    output CorePack::mem_op_enum mem_op
    // output ControllerPack::ControllerSignals ctrl_signals
);

    import CorePack::*;
    
//opcode定义
    opcode_t opcode = inst[6:0];
    parameter LOAD_OPCODE   = 7'b0000011;
    parameter IMM_OPCODE    = 7'b0010011;
    parameter AUIPC_OPCODE  = 7'b0010111;
    parameter IMMW_OPCODE   = 7'b0011011;
    parameter STORE_OPCODE  = 7'b0100011;
    parameter REG_OPCODE    = 7'b0110011;
    parameter LUI_OPCODE    = 7'b0110111;
    parameter REGW_OPCODE   = 7'b0111011;
    parameter BRANCH_OPCODE = 7'b1100011;
    parameter JALR_OPCODE   = 7'b1100111;
    parameter JAL_OPCODE    = 7'b1101111;

//第一段译码opcode
    wire inst_load   = (opcode == LOAD_OPCODE);    // 加载指令
    wire inst_auipc  = (opcode == AUIPC_OPCODE);   // AUIPC
    wire inst_immw   = (opcode == IMMW_OPCODE);    // 立即数运算 (W)
    wire inst_store  = (opcode == STORE_OPCODE);   // 存储指令
    wire inst_reg    = (opcode == REG_OPCODE);     // 寄存器-寄存器运算 (64)
    wire inst_lui    = (opcode == LUI_OPCODE);     // LUI
    wire inst_regw   = (opcode == REGW_OPCODE);    // 寄存器-寄存器运算 (W)
    wire inst_branch = (opcode == BRANCH_OPCODE);  // 条件分支
    wire inst_jalr   = (opcode == JALR_OPCODE);    // 间接跳转
    wire inst_jal    = (opcode == JAL_OPCODE);     // 无条件跳转
    wire inst_alu_ri = (opcode == IMM_OPCODE) || (opcode == IMMW_OPCODE);  // IMM / IMMW
    wire inst_alu_rr = (opcode == REG_OPCODE) || (opcode == REGW_OPCODE);  // REG / REGW
    
    wire inst_alu    = inst_alu_ri || inst_alu_rr;// 所有 ALU 指令
    wire inst_jump   = inst_jal || inst_jalr;       // 跳转指令

//funct3_t定义
    funct3_t funct3 = inst[14:12];
    parameter BEQ_FUNCT3   =   3'b000;
    parameter BNE_FUNCT3   =   3'b001;
    parameter BLT_FUNCT3   =   3'b100;
    parameter BGE_FUNCT3   =   3'b101;
    parameter BLTU_FUNCT3  =   3'b110;
    parameter BGEU_FUNCT3  =   3'b111;

    parameter LB_FUNCT3    =   3'b000;
    parameter LH_FUNCT3    =   3'b001;
    parameter LW_FUNCT3    =   3'b010;
    parameter LD_FUNCT3    =   3'b011;
    parameter LBU_FUNCT3   =   3'b100;
    parameter LHU_FUNCT3   =   3'b101;
    parameter LWU_FUNCT3   =   3'b110;

    parameter SB_FUNCT3    =   3'b000;
    parameter SH_FUNCT3    =   3'b001;
    parameter SW_FUNCT3    =   3'b010;
    parameter SD_FUNCT3    =   3'b011;

    parameter ADD_FUNCT3   =   3'b000;
    parameter SLL_FUNCT3   =   3'b001;
    parameter SLT_FUNCT3   =   3'b010;
    parameter SLTU_FUNCT3  =   3'b011;
    parameter XOR_FUNCT3   =   3'b100;
    parameter SRL_FUNCT3   =   3'b101;
    parameter OR_FUNCT3    =   3'b110;
    parameter AND_FUNCT3   =   3'b111;

funct7_t funct7 = inst[31:25];

always_comb begin  
        we_reg = '0;
        we_mem = '0;
        re_mem = '0;
        npc_sel = '0;
        immgen_op = IMM0;
        alu_op = ALU_ADD;  
        cmp_op = CMP_NO;
        alu_asel = ASEL_REG;
        alu_bsel = BSEL_REG;
        wb_sel = WB_SEL0;
        mem_op = MEM_NO;

//第二步译码
        we_reg=inst_load||inst_alu||inst_jump||inst_lui||inst_auipc;
        we_mem=inst_store;
        re_mem=inst_load;
        npc_sel=inst_branch||inst_jump;

    //alu_asel 选择
        case (1'b1)
            (inst_auipc || inst_branch || inst_jal): alu_asel = ASEL_PC;
            (inst_lui):                              alu_asel = ASEL0; 
            default:                                 alu_asel = ASEL_REG;
        endcase

    //alu_bsel选择
        case (1'b1)
            (inst_alu_ri || inst_load || inst_store || inst_lui || inst_auipc || inst_jal || inst_jalr || inst_branch): 
                alu_bsel = BSEL_IMM;
            (inst_alu_rr):                                                                              
                alu_bsel = BSEL_REG;
            default: 
                alu_bsel = BSEL_REG;
        endcase

    // wb_sel选择
        case (1'b1)
            (inst_load):          wb_sel = WB_SEL_MEM;
            (inst_jal || inst_jalr): wb_sel = WB_SEL_PC;
            default:              wb_sel = WB_SEL_ALU;
        endcase

    //immgen_op
        case (1'b1)
            (inst_load || inst_jalr || inst_alu_ri): immgen_op = I_IMM;
            (inst_store):                            immgen_op = S_IMM;
            (inst_branch):                           immgen_op = B_IMM;
            (inst_jal):                              immgen_op = UJ_IMM;
            (inst_lui || inst_auipc):                immgen_op = U_IMM;
            default:                                 immgen_op = IMM0;
        endcase

//第二段细分
        if(inst_alu)begin   //r，i型指令
            case (funct3)
                ADD_FUNCT3: begin               //兼容了sub,subw,addw,add
                    if (inst_alu_ri) begin
                        alu_op=(opcode==IMM_OPCODE)?ALU_ADD:ALU_ADDW;
                    end
                    else if (inst_alu_rr) begin
                        if(funct7==7'b0000000)begin
                            alu_op=(opcode==REG_OPCODE)?ALU_ADD:ALU_ADDW;
                        end
                        else if(funct7==7'b0100000)begin
                            alu_op=(opcode==REG_OPCODE)?ALU_SUB:ALU_SUBW;
                        end
                    end
                end
                SLL_FUNCT3: begin                //兼容了sll，sllw
                    alu_op=(opcode==IMM_OPCODE||opcode==REG_OPCODE)?ALU_SLL:ALU_SLLW;
                end
                SLT_FUNCT3:alu_op=ALU_SLT;
                SLTU_FUNCT3:alu_op=ALU_SLTU;
                XOR_FUNCT3:alu_op=ALU_XOR;
                SRL_FUNCT3:begin
                    if (inst_alu_ri) begin  //srliw,srli,srai,sraiw
                            if (funct7[5]) begin
                            alu_op=(opcode==IMM_OPCODE)?ALU_SRA:ALU_SRAW;
                        end
                            if(!funct7[5])begin
                            alu_op=(opcode==IMM_OPCODE)?ALU_SRL:ALU_SRLW;
                        end
                    end
                    else begin  //srl,sra,srlw,sraw
                        if(funct7==7'b0000000)begin
                            alu_op=(opcode==REG_OPCODE)?ALU_SRL:ALU_SRLW;
                        end
                        else if (funct7==7'b0100000) begin
                            alu_op=(opcode==REG_OPCODE)?ALU_SRA:ALU_SRAW;
                        end
                    end
                end
                OR_FUNCT3:alu_op=ALU_OR;
                AND_FUNCT3:alu_op=ALU_AND;
                default: alu_op=ALU_DEFAULT;
            endcase
        end
        
        if (inst_branch) begin //b型指令
            case (funct3)
                BEQ_FUNCT3:cmp_op=CMP_EQ;
                BNE_FUNCT3:cmp_op=CMP_NE;
                BLT_FUNCT3:cmp_op=CMP_LT;
                BGE_FUNCT3:cmp_op=CMP_GE;
                BLTU_FUNCT3:cmp_op=CMP_LTU;
                BGEU_FUNCT3:cmp_op=CMP_GEU;
                default: cmp_op=CMP_NO;
            endcase
        end        

        if (inst_load||inst_store) begin//l、s型指令
            case (funct3)
               LB_FUNCT3:mem_op=MEM_B;
               LH_FUNCT3:mem_op=MEM_H;
               LW_FUNCT3:mem_op=MEM_W;
               LD_FUNCT3:mem_op=MEM_D;
               LBU_FUNCT3:mem_op=(inst_load)?MEM_UB:MEM_NO;
               LHU_FUNCT3:mem_op=(inst_load)?MEM_UH:MEM_NO;
               LWU_FUNCT3:mem_op=(inst_load)?MEM_UW:MEM_NO;
                default: mem_op=MEM_NO;
            endcase
        end   
            
end
endmodule
