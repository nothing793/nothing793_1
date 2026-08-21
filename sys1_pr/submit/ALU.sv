`include "core_struct.vh"
module ALU (
  input  CorePack::data_t a,
  input  CorePack::data_t b,
  input  CorePack::alu_op_enum  alu_op,
  output CorePack::data_t res
);
  
  import CorePack::*;
  logic [31:0]addw_res;
  logic [31:0]subw_res;
  logic [31:0]sllw_res;
  logic [31:0]srlw_res;
  logic [31:0]sraw_res;

  always_comb begin 
      addw_res = 32'b0;
      subw_res = 32'b0;
      sllw_res = 32'b0;
      srlw_res = 32'b0;
      sraw_res = 32'b0;
      
      case (alu_op)
        ALU_ADD: res=a+b;
        ALU_SUB: res=a-b;
        ALU_AND: res=a&b;
        ALU_OR: res=a|b;
        ALU_XOR: res=a^b;
        ALU_SLT: res={63'b0,$signed(a)<$signed(b)}; //有符号比较
        ALU_SLTU: res={63'b0,a<b}; // 无符号比较
        ALU_SLL: res=a<<(b[5:0]); //a左移b低六位
        ALU_SRL: res=a>>(b[5:0]); //a右移b第六位
        ALU_SRA: res=$signed(a)>>>(b[5:0]);//算数右移
        ALU_ADDW: begin                           //32位加法
          addw_res = $signed(a[31:0]) + $signed(b[31:0]);
          res={{32{addw_res[31]}},addw_res};
        end
        ALU_SUBW: begin                           //32位减法
          subw_res = $signed(a[31:0])-$signed(b[31:0]);
          res = {{32{subw_res[31]}},subw_res};
        end
        ALU_SLLW: begin                           //32位左移
          sllw_res = a[31:0]<<(b[4:0]);
          res = {{32{sllw_res[31]}},sllw_res};
        end
        ALU_SRLW: begin                           //32位右移
          srlw_res = a[31:0]>>(b[4:0]);
          res = {32'b0,srlw_res};
        end
        ALU_SRAW: begin                           //算术右移
          sraw_res = $signed(a[31:0])>>>(b[4:0]);
          res = {{32{sraw_res[31]}},sraw_res};
        end
        default: res= '0;
      endcase

  end

endmodule
