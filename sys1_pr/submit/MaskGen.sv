`include "core_struct.vh"

module MaskGen(
    input CorePack::mem_op_enum mem_op,
    input CorePack::addr_t dmem_waddr,
    output CorePack::mask_t dmem_wmask
);

  import CorePack::*;
  logic [MASK_WIDTH-1:0] wmask_reg;

  assign dmem_wmask = wmask_reg;

  always_comb begin 
    case (mem_op)
      MEM_B,MEM_UB:
          wmask_reg=8'b1<<dmem_waddr[2:0];
      MEM_H,MEM_UH:
          wmask_reg=8'b11<<dmem_waddr[2:0];
      MEM_W,MEM_UW:
          wmask_reg=8'b1111<<dmem_waddr[2:0];
      MEM_D: 
          wmask_reg=8'hFF;
      default: 
          wmask_reg=8'b0;
    endcase
  end

endmodule
