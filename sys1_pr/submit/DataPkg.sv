`include "core_struct.vh"

module DataPkg(
    input CorePack::mem_op_enum mem_op,
    input CorePack::data_t reg_data,
    input CorePack::addr_t dmem_waddr,//低三位代表字节索引
    output CorePack::data_t dmem_wdata
);

  import CorePack::*;

  // Data package
  always_comb begin 
    dmem_wdata = '0;

    case (mem_op) 
     MEM_B,MEM_UB:
          dmem_wdata[dmem_waddr[2:0]*8+: 8]=reg_data[7:0];
     MEM_H,MEM_UH:
          dmem_wdata[dmem_waddr[2:0]*8+:16]=reg_data[15:0];
     MEM_W,MEM_UW:
          dmem_wdata[dmem_waddr[2:0]*8+:32]=reg_data[31:0];
     MEM_D: dmem_wdata=reg_data;
      default: dmem_wdata='0;
    endcase
  end

endmodule
